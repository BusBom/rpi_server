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
#include <deque>
#include <thread>
#include <stdexcept>

int main() {
    // --- 1. 초기 설정 ---
    BusStationManager manager;
    const std::string stop_status_cgi_url = "https://192.168.219.50/cgi-bin/stop-status.cgi";  // platform_observer
    const std::string bus_mapping_cgi_url = "https://127.0.0.1/cgi-bin/bus-mapping.cgi";     // local data
    
    std::map<int, int> pending_assignments; // {platform_id -> bus_id} 배차 지시된 버스목록
    std::deque<int> assigned_buses; // 현재 플랫폼에 배치 대기중인 버스들의 순서
    std::map<int, int> platform_to_bus; // {platform_index -> bus_id} 실제 정차한 버스 기록
    std::vector<int> previous_status; // 이전 플랫폼 상태 기록
    bool is_initialized = false;
    
    int previous_outgoings = 0; // 이전 출차 횟수를 기록

    int incommings;
    int outgoings;

    // --- 2. 메인 루프 시작 ---
    while (true) {
        try {
            StopStatusData data = fetchStopStatusFromHTTP(stop_status_cgi_url);             // 정류장 상태 정보
            std::deque<int> json_bus_queue = fetchIncomingBusQueue(bus_mapping_cgi_url);    // 곧 도착할 버스 목록 (현재 접근중인 버스들)
            std::vector<std::pair<int, std::string>> display_result; // 최종 출력 결과
            std::vector<int> fetchedStatus = data.platform_status;
            incommings = 5;
            outgoings = 4;

            std::cout << "Exited Bus Count: " << outgoings << std::endl;
            std::cout << "Entered Bus Count: " << incommings << std::endl;

            // 초기화
            if (!is_initialized) {
                previous_status = fetchedStatus;
                is_initialized = true;
            }

            // 플랫폼 상태 변화 감지 및 처리
            for (int i = 0; i < fetchedStatus.size() && i < previous_status.size(); i++) {
                if (previous_status[i] == 0 && fetchedStatus[i] == 1) {
                    // 빈 플랫폼에 버스가 정차함 (0 → 1)
                    if (!assigned_buses.empty()) {
                        int entered_bus = assigned_buses.front();
                        assigned_buses.pop_front();
                        platform_to_bus[i] = entered_bus;
                        std::cout << "Bus " << entered_bus << " entered platform " << (i+1) << std::endl;
                    }
                } else if (previous_status[i] == 1 && fetchedStatus[i] == 0) {
                    // 버스가 플랫폼에서 출발함 (1 → 0)
                    if (platform_to_bus.find(i) != platform_to_bus.end()) {
                        int departed_bus = platform_to_bus[i];
                        platform_to_bus.erase(i);
                        std::cout << "Bus " << departed_bus << " departed from platform " << (i+1) << std::endl;
                    }
                }
            }
            
            // 이전 상태 업데이트
            previous_status = fetchedStatus;

            // 출차가 발생했는지 확인 (이제 플랫폼 상태로 처리되므로 이 로직은 보조적)
            if (outgoings > previous_outgoings) {
                int buses_to_remove = outgoings - previous_outgoings;
                std::cout << "Total buses departed: " << buses_to_remove << std::endl;
                previous_outgoings = outgoings;
            }

            // 현재 접근중인 버스들을 incom_bus로 설정 (중복 제거)
            std::deque<int> incom_bus;
            for (int bus_id : json_bus_queue) {
                // incom_bus에 해당 bus_id가 없고, 이미 배치 대기중이거나 정차한 버스도 아니면 추가
                bool already_exists = (std::find(incom_bus.begin(), incom_bus.end(), bus_id) != incom_bus.end()) ||
                                    (std::find(assigned_buses.begin(), assigned_buses.end(), bus_id) != assigned_buses.end());
                
                // 이미 정차한 버스인지 확인
                bool already_stationed = false;
                for (const auto& pair : platform_to_bus) {
                    if (pair.second == bus_id) {
                        already_stationed = true;
                        break;
                    }
                }
                
                if (!already_exists && !already_stationed) {
                    incom_bus.push_back(bus_id);
                }
            }
            
            // 빈 플랫폼에 새로운 버스 배치 (대기 큐에 추가)
            int empty_platforms = 0;
            for (int status : fetchedStatus) {
                if (status == 0) empty_platforms++;
            }
            
            // 현재 정차한 버스 수 계산
            int stationed_buses = platform_to_bus.size();
            
            // 배치 가능한 플랫폼 수 = 빈 플랫폼 수 - 현재 대기중인 버스 수
            int available_spots = empty_platforms - assigned_buses.size();
            
            // 새 버스를 대기 큐에 추가
            while (available_spots > 0 && !incom_bus.empty()) {
                int new_bus = incom_bus.front();
                incom_bus.pop_front();
                assigned_buses.push_back(new_bus);
                std::cout << "Bus " << new_bus << " added to waiting queue" << std::endl;
                available_spots--;
            }
            // incom_bus 상태 출력 (디버깅용)
            std::cout << "Current approaching buses: ";
            for (int bus : incom_bus) {
                std::cout << bus << " ";
            }
            std::cout << std::endl;

            std::cout << "Currently waiting buses: ";
            for (int bus : assigned_buses) {
                std::cout << bus << " ";
            }
            std::cout << std::endl;

            std::cout << "Currently stationed buses: ";
            for (const auto& pair : platform_to_bus) {
                std::cout << "P" << (pair.first + 1) << ":" << pair.second << " ";
            }
            std::cout << std::endl;

            std::cout << "Fetched Status: ";
            for (int status : fetchedStatus) {
                std::cout << status << " ";
            }  
            std::cout << std::endl;

            // display_result 생성
            display_result.clear();
            for (int i = 0; i < fetchedStatus.size(); i++) {
                display_result.push_back({i + 1, " "}); // platform은 1부터 시작
            }

            // 먼저 실제 정차한 버스들을 배치
            for (const auto& pair : platform_to_bus) {
                int platform_index = pair.first;
                int bus_id = pair.second;
                if (platform_index < display_result.size()) {
                    display_result[platform_index].second = std::to_string(bus_id);
                }
            }

            // 그 다음 빈 플랫폼에 순서대로 대기중인 버스들을 할당
            int bus_index = 0;
            for (int i = 0; i < fetchedStatus.size() && bus_index < assigned_buses.size(); i++) {
                if (fetchedStatus[i] == 0 && platform_to_bus.find(i) == platform_to_bus.end()) {
                    // 빈 플랫폼이면서 실제 정차한 버스가 없는 경우
                    display_result[i].second = std::to_string(assigned_buses[bus_index]);
                    bus_index++;
                }
            }
            
            // 결과 출력 (디버깅용)
            std::cout << "Display Result: ";
            for (const auto& platform : display_result) {
                std::cout << "{" << platform.first << ", " << platform.second << "} ";
            }
            std::cout << std::endl;
            
            std::cout << "Remaining approaching buses: ";
            for (int bus : incom_bus) {
                std::cout << bus << " ";
            }
            std::cout << std::endl;

            writeResultToDevice(display_result);
            
        } catch (const std::runtime_error& e) {
            std::cerr << "[Error] " << e.what() << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    return 0;
}