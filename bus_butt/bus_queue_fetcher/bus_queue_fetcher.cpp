#include "bus_queue_fetcher.h"
#include <iostream>
#include <stdexcept>

// --- 외부 라이브러리 헤더 ---
#include <curl/curl.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

/**
 * @brief libcurl이 HTTP 응답 데이터를 저장하기 위해 사용하는 콜백 함수.
 */
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

/**
 * @brief bus-mapping.cgi에 HTTP 요청을 보내 정류장으로 접근 중인 버스 목록을 가져옵니다.
 * @param url bus-mapping.cgi의 전체 주소.
 * @return std::list<int> 정수형 버스 ID 목록.
 */
std::list<int> fetchIncomingBusQueue(const std::string& url) {

    CURL* curl;
    CURLcode res;
    std::string readBuffer;

    curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to init curl for bus queue");
    }

    // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

    curl_easy_setopt(curl, CURLOPT_SSLCERT, "/etc/nginx/ssl/server1.cert.pem");
    curl_easy_setopt(curl, CURLOPT_SSLKEY, "/etc/nginx/ssl/server1.key.pem");
    curl_easy_setopt(curl, CURLOPT_CAINFO, "/etc/nginx/ssl/ca.cert.pem");

    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);  // 인증서 검증 비활성화
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);  // 호스트 검증 비활성화

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2L);

    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        curl_easy_cleanup(curl);
        throw std::runtime_error("curl_easy_perform() [BusQueue]failed: " + std::string(curl_easy_strerror(res)));
    }
    curl_easy_cleanup(curl);

    // 디버깅: HTTP 응답 원본 데이터 출력
    // std::cout << "[Debug] Raw HTTP response: " << readBuffer << std::endl;

    std::list<int> queue;
    try {
        auto j = json::parse(readBuffer);
        
        // 디버깅: 파싱된 JSON 구조 출력
        // std::cout << "[Debug] Parsed JSON: " << j.dump(2) << std::endl;
        // std::cout << "[Debug] JSON array size: " << j.size() << std::endl;
        
        for (const auto& item : j) {
            // 디버깅: 각 아이템 정보 출력
            // std::cout << "[Debug] Processing item: " << item.dump() << std::endl;
            
            // busNumber가 문자열이므로 stoi를 사용해 int로 변환
            queue.push_back(std::stoi(item.at("routeID").get<std::string>()));
        }
        std::cout << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "[Error] Failed to parse bus queue JSON: " << e.what() << std::endl;
        std::cerr << "[Error] Raw response was: " << readBuffer << std::endl;
        // 파싱에 실패하더라도 빈 큐를 반환하여 프로그램이 중단되지 않도록 함
    }
    return queue;
}