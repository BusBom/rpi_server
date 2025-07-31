#include "stop_status_fetcher.h"
#include "bus_queue_fetcher.h"
#include "display_writer.h"
#include "bus_station_manager.h"

#include <unistd.h>  // sleep
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <list>
#include <chrono>
#include <algorithm>
#include <thread>
#include <stdexcept>

int main() {
    // --- 1. 초기 설정 ---
    BusStationManager manager;
    const std::string stop_status_cgi_url = "https://192.168.0.82/cgi-bin/stop-status.cgi";  // platform_observer
    const std::string bus_mapping_cgi_url = "https://127.0.0.1/cgi-bin/bus-mapping.cgi";     // local data
    
    std::map<int, int> pending_assignments; // {platform_id -> bus_id} 배차 지시된 버스목록
    bool is_initialized = false;

    // --- 2. 메인 루프 시작 ---
    while (true) {
        try {
            StopStatusData data = fetchStopStatusFromHTTP(stop_status_cgi_url);             // 정류장 상태 정보
            std::list<int> incoming_bus_queue = fetchIncomingBusQueue(bus_mapping_cgi_url); // 곧 도착할 버스 목록

            std::cout << "\n[Fetch] " << data.updated_at << " / Status: ";                  // 업데이트 시각
            for(int s : data.platform_status) std::cout << s << " ";
            std::cout << std::endl;
            
            // 유효 플랫폼 개수 동적 계산
            int total_valid_platforms = 0;
            for (int i = data.platform_status.size() - 1; i >= 0; --i) {
                if (data.platform_status[i] != -1) {
                    total_valid_platforms = i + 1;
                    break;
                }
            }

            // 모든 플랫폼이 -1인 경우, 루프를 계속하지만 아무 작업도 하지 않음
            if (total_valid_platforms == 0 && !data.platform_status.empty()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                continue;
            }

            // 센서 상태 업데이트 및 안정화 확인
            manager.updateState(data.platform_status);
            
            // 초기 상태 동기화
            if (!is_initialized && manager.isStable()) {
                std::cout << "[System] 초기 상태 동기화 시작...\n";
                ConfirmedActions initial_actions = manager.analyzeStableState();
                for (int platform : initial_actions.new_arrivals_on_platforms) {
                    manager.setBusOnPlatform(platform, -1);
                    std::cout << "  - 플랫폼 " << platform << "에 미확인 버스 존재 확인.\n";
                }
                manager.applyActions(initial_actions);
                is_initialized = true;
                std::cout << "[System] 초기 상태 동기화 완료.\n";
            }

            // 시스템 안정 시, 이벤트 처리 및 신규 배차
            if (is_initialized && manager.isStable()) {
                // A. 발생한 이벤트 목록 가져오기
                ConfirmedActions actions = manager.analyzeStableState();

                std::vector<std::pair<int, std::string>> instructions;

                // B. 확정 이벤트가 있을 경우 처리
                if (!actions.empty()) {
                    std::cout << "\n<<<<< 확정된 이벤트 처리 >>>>>\n";
                    
                    // 출차 이벤트
                    std::map<int, int> all_departures = actions.new_departures;
                    all_departures.insert(actions.truly_departed_buses.begin(), actions.truly_departed_buses.end());

                    for (const auto& [platform, bus_id] : all_departures) {
                        instructions.push_back({platform, ""}); // 공백 문자로 출차 알림
                        std::string bus_name = (bus_id == -1) ? "미확인 버스" : std::to_string(bus_id);
                        std::cout << "  - [출차] " << bus_name << "가 플랫폼 " << platform << "을(를) 떠났습니다.\n";
                    }

                    // 재정차 이벤트
                    for (const auto& [platform, bus_id] : actions.reentries) {
                        std::string bus_name = (bus_id == -1) ? "미확인 버스" : std::to_string(bus_id);
                        std::cout << "  - [재정차] " << bus_name << "가 플랫폼 " << platform << "으로 이동했습니다.\n";
                    }

                    // 신규 도착 이벤트 처리
                    for (int arrived_platform : actions.new_arrivals_on_platforms) {
                        int bus_id = -1;
                        int platform_to_confirm = -1;

                        // 지시한 플랫폼과 같은 위치인지 확인
                        if (pending_assignments.count(arrived_platform)) {
                            platform_to_confirm = arrived_platform;
                        } else if (pending_assignments.count(arrived_platform - 1)) {
                            platform_to_confirm = arrived_platform - 1;
                        } else if (pending_assignments.count(arrived_platform + 1)) {
                            platform_to_confirm = arrived_platform + 1;
                        }

                        if (platform_to_confirm != -1) {
                            bus_id = pending_assignments.at(platform_to_confirm);
                            manager.setBusOnPlatform(arrived_platform, bus_id);
                            pending_assignments.erase(platform_to_confirm);
                            std::cout << "  - [도착 확인] 버스 " << bus_id << "가 플랫폼 " << arrived_platform << "에 도착했습니다. (원래 지시: " << platform_to_confirm << ")\n";
                        } else {
                            manager.setBusOnPlatform(arrived_platform, -1);
                            std::cout << "  - [경고] 예상치 못한 미확인 버스가 플랫폼 " << arrived_platform << "에 도착했습니다.\n";
                        }
                    }
                    // 분석된 모든 이벤트를 실제 시스템 상태에 최종 적용
                    manager.applyActions(actions);
                }
                
                // C. 신규 배차 로직 (선입선출)
                std::vector<int> assignable_slots;
                for (int i = total_valid_platforms - 1; i >= 0; --i) {
                    if (data.platform_status[i] == 0 && pending_assignments.count(i) == 0) {
                        assignable_slots.push_back(i);
                    } else {
                        break;
                    }
                }
                std::sort(assignable_slots.begin(), assignable_slots.end());
                
                // incoming queue에 있는 버스 꺼내기
                for (int platform : assignable_slots) {
                    if (!incoming_bus_queue.empty()) {
                        int bus_to_assign = incoming_bus_queue.front();
                        incoming_bus_queue.pop_front();
                        pending_assignments[platform] = bus_to_assign;
                        instructions.push_back({platform, std::to_string(bus_to_assign)});
                    }
                }

                // D. 생성된 지시사항이 있으면 드라이버로 전송
                if (!instructions.empty()) {
                    std::vector<std::pair<int, std::string>> final_instructions;
                    auto current_bus_map = manager.getPlatformBusMap();
                    
                    // 1. 현재 확정된 버스 상태를 추가
                    for(const auto& [plat, bus_id] : current_bus_map) {
                        if (bus_id == -1) {
                            final_instructions.push_back({plat, ""});
                        } else {
                            final_instructions.push_back({plat, std::to_string(bus_id)});
                        }
                    }
                    // 2. 새로 발생한 지시사항(출차, 신규배차)으로 덮어쓰기
                    for(const auto& inst : instructions) {
                        bool found = false;
                        for(auto& final_inst : final_instructions) {
                            if (final_inst.first == inst.first) {
                                final_inst.second = inst.second;
                                found = true;
                                break;
                            }
                        }
                        if (!found) {
                            final_instructions.push_back(inst);
                        }
                    }
                    printResultToSHM(final_instructions, total_valid_platforms);
                    writeResultToDevice(final_instructions);    // 현재 LED Matrix는 플랫폼 4개까지만 출력. 추후 확장 가능
                }

            }

        } catch (const std::runtime_error& e) {
            std::cerr << "[Error] " << e.what() << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    return 0;
}