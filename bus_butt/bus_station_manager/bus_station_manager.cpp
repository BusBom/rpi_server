#include "bus_station_manager.h"

BusStationManager::BusStationManager() {}

void BusStationManager::setBusOnPlatform(int platform_id, int bus_id) {
    platform_to_bus_[platform_id] = bus_id;
}

void BusStationManager::removeBusFromPlatform(int platform_id) {
    platform_to_bus_.erase(platform_id);
}

const std::map<int, int>& BusStationManager::getPlatformBusMap() const {
    return platform_to_bus_;
}

/**
* @brief 현재 점유된 플랫폼 상태를 0과 1로 구성된 벡터로 반환.
*/
std::vector<int> BusStationManager::getOccupiedPlatforms(int total_platforms) const {
    std::vector<int> status(total_platforms, 0);
    for(const auto& pair : platform_to_bus_) {
        if (pair.first < status.size()) {
            status[pair.first] = 1;
        }
    }
    return status;
}