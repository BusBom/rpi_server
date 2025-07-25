#pragma once

#include <vector>
#include <string>
#include "stop_status.hpp"  // StopStatus 구조체 사용을 위해 include

// string 버스 번호 목록 반환
std::vector<std::string> readSequenceFromSHM();

// stop-status.cgi JSON 결과에서 정류장 상태 가져오기
std::vector<int> fetchStopStatusFromCGI();

// 공유 메모리에서 StopStatus 직접 읽기 
StopStatus readStopStatusFromSHM();
