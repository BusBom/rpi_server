#include "bus_station_manager.h"

// ConfirmedActions 구조체의 empty 함수
bool ConfirmedActions::empty() const {
    return reentries.empty() && new_arrivals_on_platforms.empty() && truly_departed_buses.empty() && new_departures.empty();
}

// BusStationManager 클래스의 생성자
BusStationManager::BusStationManager() {
    last_sensor_change_time_ = std::chrono::steady_clock::now();
}

// 시스템이 안정적인지 확인하는 함수
bool BusStationManager::isStable() const {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(now - last_sensor_change_time_).count() >= CONFIRMATION_TIME_S;
}

// 특정 플랫폼에 버스를 수동으로 배정하는 함수
void BusStationManager::setBusOnPlatform(int platform_id, int bus_id) {
    platform_to_bus_[platform_id] = bus_id;
}

// 현재 버스-플랫폼 할당 맵을 반환하는 함수
const std::map<int, int>& BusStationManager::getPlatformBusMap() const {
    return platform_to_bus_;
}

// 현재 점유된 플랫폼 상태를 벡터로 반환하는 함수
std::vector<int> BusStationManager::getOccupiedPlatforms(int total_platforms) const {
    std::vector<int> status(total_platforms, 0);
    for(const auto& pair : platform_to_bus_) {
        if (pair.first < status.size()) {
            status[pair.first] = 1;
        }
    }
    return status;
}

// 최신 센서 상태를 업데이트하는 함수
void BusStationManager::updateSensorState(const std::vector<int>& current_status) {
    if (last_seen_sensor_status_.size() != current_status.size()) {
        last_seen_sensor_status_.resize(current_status.size(), 0);
    }
    if (last_seen_sensor_status_ != current_status) {
        last_seen_sensor_status_ = current_status;
        last_sensor_change_time_ = std::chrono::steady_clock::now();
        std::cout << "[Sensor] 상태 변경 감지. 안정화 타이머 리셋.\n";
    }
}

// 안정화된 상태를 분석하는 함수
ConfirmedActions BusStationManager::analyzeStableState() const {
    auto now = std::chrono::steady_clock::now();
    ConfirmedActions actions;
    
    for (const auto& info : departed_pool_) {
        if (std::chrono::duration_cast<std::chrono::seconds>(now - info.time).count() >= REENTRY_COOLDOWN_S) {
            actions.truly_departed_buses[info.from_platform] = info.bus_id;
        }
    }

    const auto& stable_status = last_seen_sensor_status_;
    std::vector<int> previous_logical_status = getOccupiedPlatforms(stable_status.size());
    
    std::map<int, int> departure_candidates;

    for (int i = 0; i < stable_status.size(); ++i) {
        if (stable_status[i] == -1) continue;

        if (previous_logical_status[i] == 1 && stable_status[i] == 0) {
            if (platform_to_bus_.count(i)) {
                departure_candidates[i] = platform_to_bus_.at(i);
            }
        } else if (previous_logical_status[i] == 0 && stable_status[i] == 1) {
            bool is_reentry = false;
            for (const auto& dep_info : departed_pool_) {
                actions.reentries[i] = dep_info.bus_id;
                is_reentry = true;
                break;
            }
            if (!is_reentry) {
                actions.new_arrivals_on_platforms.push_back(i);
            }
        }
    }
    
    actions.new_departures = departure_candidates;
    return actions;
}

// 분석된 조치를 실제 상태에 적용하는 함수
void BusStationManager::applyActions(const ConfirmedActions& actions) {
    auto now = std::chrono::steady_clock::now();

    for (const auto& [platform, bus_id] : actions.truly_departed_buses) {
        departed_pool_.remove_if([&](const DepartedInfo& info){ return info.bus_id == bus_id; });
    }

    for (const auto& [platform, bus_id] : actions.new_departures) {
        platform_to_bus_.erase(platform);
        DepartedInfo new_departure;
        new_departure.bus_id = bus_id;
        new_departure.from_platform = platform;
        new_departure.time = now;
        departed_pool_.push_back(new_departure);
    }

    for (const auto& [to_platform, bus_id] : actions.reentries) {
        departed_pool_.remove_if([&](const DepartedInfo& info){ return info.bus_id == bus_id; });
        platform_to_bus_[to_platform] = bus_id;
    }

    last_confirmed_status_ = last_seen_sensor_status_;
}