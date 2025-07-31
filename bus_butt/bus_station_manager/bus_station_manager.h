#ifndef BUS_STATION_MANAGER_H
#define BUS_STATION_MANAGER_H

#include <vector>
#include <string>
#include <map>
#include <list>
#include <chrono>
#include <iostream>

/**
 * @brief 출차한 버스의 정보(ID, 떠난 플랫폼, 시간)를 담는 구조체.
 */
struct DepartedInfo {
    int bus_id;
    int from_platform;
    std::chrono::time_point<std::chrono::steady_clock> time;
};

/**
 * @brief 안정화가 끝난 후 확정된 조치 사항들을 담는 구조체.
 */
struct ConfirmedActions {
    std::map<int, int> reentries;               // {도착 플랫폼, 버스 ID}
    std::vector<int> new_arrivals_on_platforms; // 신규 버스가 도착한 플랫폼 목록
    std::map<int, int> truly_departed_buses;    // {떠난 플랫폼, 버스 ID} (타임아웃된 버스)
    std::map<int, int> new_departures;          // {떠난 플랫폼, 버스 ID} (방금 떠난 버스)

    bool empty() const;
};

/**
 * @class BusStationManager
 * @brief 정류장의 전체 논리적 상태를 관리하고, 정류장 상태 데이터 변화를 분석하여
 * 출차, 재정차, 신규 도착을 결정하는 핵심 클래스.
 */
class BusStationManager {
private:
    // --- 실제 시스템 상태 ---
    std::map<int, int> platform_to_bus_;        // 현재 어느 플랫폼에 어떤 버스가 있는지 관리 (핵심 큐)
    std::list<DepartedInfo> departed_pool_;     // 출차 후 재정차 가능성이 있는 버스 목록

    // --- 안정화용 변수 ---
    std::vector<int> last_seen_state_status_;  // 마지막으로 확인한 정류장 상태
    std::chrono::time_point<std::chrono::steady_clock> last_state_change_time_; // 정류장 상태가 마지막으로 바뀐 시간
    std::vector<int> last_confirmed_status_;    // 마지막으로 '확정' 처리한 상태 (중복 처리 방지용)

    const int REENTRY_COOLDOWN_S = 5;           // 재정차로 간주할 최대 시간 (초)
    const int CONFIRMATION_TIME_S = 2;          // 정류장 상태 안정화에 필요한 시간 (초)

public:
    BusStationManager();

    void setBusOnPlatform(int platform_id, int bus_id);
    const std::map<int, int>& getPlatformBusMap() const;
    std::vector<int> getOccupiedPlatforms(int total_platforms) const;
    void updateState(const std::vector<int>& current_status);
    ConfirmedActions analyzeStableState() const;
    void applyActions(const ConfirmedActions& actions);
    bool isStable() const;
};

#endif // BUS_STATION_MANAGER_H