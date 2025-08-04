#include "stop_status_fetcher.h"
#include "bus_queue_fetcher.h"
#include "display_writer.h"
#include "bus_station_manager.h"

#include <unistd.h>
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <list>
#include <chrono>
#include <algorithm>
#include <thread>
#include <stdexcept>
#include <set>

int main() {
    // --- 1. 초기 설정 ---
    BusStationManager manager;
    const std::string stop_status_cgi_url = "https://192.168.219.82/cgi-bin/stop-status.cgi";
    const std::string bus_mapping_cgi_url = "https://localhost/cgi-bin/bus-mapping.cgi";
    
    std::map<int, int> pending_assignments; // 정류장 배치 상황 저장용
    bool is_initialized = false;
    int last_exited_count = 0;
    int stacked_outgoing = 0;
    
    std::cout << "메인 서버 로직 시작. 0.5초마다 정류장 상태를 확인합니다.\n";
    
    // --- 2. 메인 루프 시작 ---
    while (true) {
        try {
            // --- 단계 1: 데이터 수집 ---
            StopStatusData data = fetchStopStatusFromHTTP(stop_status_cgi_url);
            std::list<int> incoming_bus_queue = fetchIncomingBusQueue(bus_mapping_cgi_url);

            std::cout << "\n[Fetch] " << data.updated_at << " / Status: ";
            for(int s : data.platform_status) std::cout << s << " ";
            std::cout << std::endl;
            
            // --- 단계 2: 유효 플랫폼 개수 계산 ---
            const int total_valid_platforms = data.platform_status.size();

            if (total_valid_platforms == 0 && !data.platform_status.empty()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                continue;
            }

            // --- 단계 3: 이벤트 처리 ---
            bool needs_display_update = false;

            // 초기 상태 동기화
            if (!is_initialized) {
                std::cout << "[System] 초기 상태 동기화...\n";
                for(int i=0; i < total_valid_platforms; ++i) {
                    if(data.platform_status[i] == 1) {
                        manager.setBusOnPlatform(i, -1); // 미확인 버스
                    }
                }
                last_exited_count = data.exited_bus_count;
                is_initialized = true;
                needs_display_update = true;
            }

            // 출차 이벤트 로그 출력
            int departure_count = data.exited_bus_count - last_exited_count;
            if (departure_count > 0) {
                std::cout << "\n<<<<< 출차 이벤트 감지 (" << departure_count << "대) >>>>>\n";
                stacked_outgoing += departure_count;
                
                // 현재 플랫폼-버스 맵에서 앞쪽 플랫폼부터 출차된 개수만큼 제거
                auto current_bus_map = manager.getPlatformBusMap();
                std::vector<int> occupied_platforms;
                for (const auto& [platform, bus_id] : current_bus_map) {
                    occupied_platforms.push_back(platform);
                }
                std::sort(occupied_platforms.begin(), occupied_platforms.end());
                
                // 앞쪽 플랫폼부터 departure_count만큼 제거
                for (int i = 0; i < stacked_outgoing && i < occupied_platforms.size(); ++i) {
                    int platform_to_remove = occupied_platforms[i];
                    manager.removeBusFromPlatform(platform_to_remove);
                    std::cout << "  - [출차 처리] 플랫폼 " << platform_to_remove << "에서 버스 제거\n";
                }
                
                last_exited_count = data.exited_bus_count;
                needs_display_update = true;
            }
            
            // 도착 이벤트 (출차 처리 후 최신 상태로 다시 가져오기)
            std::vector<int> current_logical_status = manager.getOccupiedPlatforms(total_valid_platforms);
            
            // 안전성 체크: 벡터 크기가 예상과 다르면 스킵
            if (current_logical_status.size() != static_cast<size_t>(total_valid_platforms)) {
                std::cerr << "[Warning] 논리적 상태 벡터 크기 불일치. 예상: " << total_valid_platforms 
                         << ", 실제: " << current_logical_status.size() << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                continue;
            }
            
            for(int i=0; i < total_valid_platforms; ++i) {
                // 논리적으로는 비어있었는데, 물리적으로 채워진 플랫폼을 찾아 도착 처리
                // 단, 물리적으로도 실제 점유된 상태(1)인 경우만 처리
                if(current_logical_status[i] == 0 && data.platform_status[i] == 1) {
                    needs_display_update = true;
                    int bus_id = -1;
                    int platform_to_confirm = -1;

                    // 부정확한 주차를 고려하여 주변 플랫폼까지 확인
                    if (pending_assignments.count(i)) {
                        platform_to_confirm = i;
                    } else if (i > 0 && pending_assignments.count(i - 1)) {
                        platform_to_confirm = i - 1;
                    } else if (i < total_valid_platforms - 1 && pending_assignments.count(i + 1)) {
                        platform_to_confirm = i + 1;
                    }

                    if (platform_to_confirm != -1) {
                        bus_id = pending_assignments.at(platform_to_confirm);
                        manager.setBusOnPlatform(i, bus_id);
                        pending_assignments.erase(platform_to_confirm);
                        std::cout << "  - [도착 확인] 버스 " << bus_id << "가 플랫폼 " << i << "에 도착했습니다. (원래 지시: " << platform_to_confirm << ")\n";
                    } else {
                        // 실제 물리적 점유가 확인된 경우에만 미확인 버스로 설정
                        manager.setBusOnPlatform(i, -1);
                        std::cout << "  - [경고] 예상치 못한 미확인 버스가 플랫폼 " << i << "에 도착했습니다.\n";
                    }
                }
            }

            // --- 단계 4: 신규 배차 ---
            std::set<int> managed_bus_ids;
            auto current_bus_map = manager.getPlatformBusMap();
            for (const auto& pair : current_bus_map) managed_bus_ids.insert(pair.second);
            for (const auto& pair : pending_assignments) managed_bus_ids.insert(pair.second);

            incoming_bus_queue.remove_if([&](int id){ return managed_bus_ids.count(id) > 0; });

            // 사용 가능한 플랫폼 찾기
            std::vector<int> assignable_slots;
            for (int i = total_valid_platforms - 1; i >= 0; --i) {
                if (data.platform_status[i] == 0 && pending_assignments.count(i) == 0) {// 현재 비어있고 배차 목록에 없으면 사용 가능
                    assignable_slots.push_back(i);
                } else {
                    break;
                }
            }
            std::sort(assignable_slots.begin(), assignable_slots.end());
            
            for (int platform : assignable_slots) {
                if (!incoming_bus_queue.empty()) {
                    int bus_to_assign = incoming_bus_queue.front();
                    incoming_bus_queue.pop_front();
                    pending_assignments[platform] = bus_to_assign;
                    needs_display_update = true;
                }
            }

            // --- 단계 5: 최종 지시사항 전송 ---
            if (needs_display_update) {
                std::vector<std::pair<int, std::string>> final_instructions;
                
                // 출차/도착 처리 후 최신 상태를 다시 가져오기
                auto final_bus_map = manager.getPlatformBusMap();
                
                // 디버그: 현재 관리 중인 버스 상태 출력
                std::cout << "[Debug] 현재 관리 중인 버스: ";
                for(const auto& [plat, bus_id] : final_bus_map) {
                    std::cout << "P" << plat << ":" << bus_id << " ";
                }
                std::cout << std::endl;
                
                // 1. 현재 확정된 버스 상태를 추가
                for(const auto& [plat, bus_id] : final_bus_map) {
                    final_instructions.push_back({plat, (bus_id == -1 ? " " : std::to_string(bus_id))});
                }

                // for (int i = 0; i < total_valid_platforms; ++i) {
                //     int bus_id = manager.getBusOnPlatform(i);  // -1이면 비어있음
                //     final_instructions.push_back({i, (bus_id == -1 ? " " : std::to_string(bus_id))});
                // }
                
                // 2. 아직 도착하지 않은 배차 지시(pending)를 덮어쓰거나 추가
                // for(const auto& [plat, bus_id] : pending_assignments) {
                //     bool found = false;
                //     for(auto& final_inst : final_instructions) {
                //         if (final_inst.first == plat) {
                //             final_inst.second = std::to_string(bus_id);
                //             found = true;
                //             break;
                //         }
                //     }
                //     if (!found) {
                //         final_instructions.push_back({plat, std::to_string(bus_id)});
                //     }
                // }

                // 2. 아직 도착하지 않은 배차 지시(pending)를 덮어쓰거나 추가
                for(const auto& [plat, bus_id] : pending_assignments) {
                    bool found = false;
                    for(auto& final_inst : final_instructions) {
                        if (final_inst.first == plat) {
                            final_inst.second = std::to_string(bus_id);
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        final_instructions.push_back({plat, std::to_string(bus_id)});
                    }
                }

                printResultToSHM(final_instructions, total_valid_platforms);
                writeResultToDevice(final_instructions, total_valid_platforms);
            }

        } catch (const std::runtime_error& e) {
            std::cerr << "[Error] " << e.what() << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    return 0;
}