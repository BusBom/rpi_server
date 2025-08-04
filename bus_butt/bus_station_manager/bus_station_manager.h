#ifndef BUS_STATION_MANAGER_H
#define BUS_STATION_MANAGER_H

#include <vector>
#include <string>
#include <map>
#include <list>
#include <chrono>


/**
 * @class BusStationManager
 * @brief 정류장의 전체 논리적 상태를 관리하고, 안정화된 센서 데이터를 분석하여
 * 필요한 조치(출차, 재정차, 신규 도착)를 결정하는 핵심 클래스입니다.
 * @details 이 클래스는 정류장의 '두뇌' 역할을 하며, 모든 판단과 상태 변경을 책임집니다.
 */
class BusStationManager {
private:
    std::map<int, int> platform_to_bus_;    // 현재 어느 플랫폼에 어떤 버스가 있는지 저장 {플랫폼 ID, 버스 ID} 

public:
    BusStationManager();    //생성자

    void setBusOnPlatform(int platform_id, int bus_id);
    void removeBusFromPlatform(int platform_id);
    const std::map<int, int> getPlatformBusMap() const;

    std::vector<int> getOccupiedPlatforms(int total_platforms) const;
};

#endif // BUS_STATION_MANAGER_H