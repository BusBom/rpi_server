#ifndef BUS_QUEUE_FETCHER_H
#define BUS_QUEUE_FETCHER_H

#include <string>
#include <deque>
/**
 * @brief bus-mapping.cgi에 HTTP 요청을 보내 정류장으로 접근 중인 버스 목록을 가져옵니다.
 * @param url bus-mapping.cgi의 전체 주소.
 * @return std::list<int> 정수형 버스 ID 목록.
 */
std::deque<int> fetchIncomingBusQueue(const std::string& url);

#endif // BUS_QUEUE_FETCHER_H