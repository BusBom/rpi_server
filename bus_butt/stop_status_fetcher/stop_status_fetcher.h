#pragma once

#include <string>
#include <vector>

// 정류장 서버 CGI에서 받아온 플랫폼 상태 정보를 담는 구조체
struct StopStatusData {
    std::string station_id;
    std::vector<int> platform_status;
    std::string updated_at;  // 문자열로 받음 ("YYYY-MM-DD HH:MM:SS")

    int current_bus_count;  // 현재 정류장 내 버스 수
    int entered_bus_count;  // 누적 진입 버스 수
    int exited_bus_count;   // 누적 진출 버스 수
};

/**
 * @brief 정류장 서버의 CGI 주소로 HTTP 요청을 보내고 플랫폼 상태를 가져옵니다.
 * @param url CGI의 전체 주소 (예: http://station001.local/cgi-bin/stop-status.cgi)
 * @return StopStatusData 구조체에 담긴 플랫폼 점유 정보 및 타임스탬프
 * @throw std::runtime_error 요청 실패 또는 파싱 실패 시
 */
StopStatusData fetchStopStatusFromHTTP(const std::string& url);