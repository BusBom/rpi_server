#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <stdexcept>
#include <sstream>

using json = nlohmann::json;

// 정류장 서버 CGI에서 받아온 플랫폼 상태 정보를 담는 구조체
struct StopStatusData {
    std::string station_id;
    std::vector<int> platform_status;
    std::string updated_at;
};

// libcurl이 응답 데이터를 받을 버퍼
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

/**
 * @brief 정류장 서버의 CGI 주소로 HTTP 요청을 보내고 플랫폼 상태를 가져옵니다.
 * @param url CGI의 전체 주소
 * @return StopStatusData 구조체에 담긴 플랫폼 점유 정보 및 타임스탬프
 * @throw std::runtime_error 요청 실패 또는 파싱 실패 시
 */
StopStatusData fetchStopStatusFromHTTP(const std::string& url) {
    CURL* curl;
    CURLcode res;
    std::string readBuffer;

    curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to init curl");
    }

    // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

    curl_easy_setopt(curl, CURLOPT_SSLCERT, "/etc/nginx/ssl/server1.cert.pem");
    curl_easy_setopt(curl, CURLOPT_SSLKEY, "/etc/nginx/ssl/server1.key.pem");
    curl_easy_setopt(curl, CURLOPT_CAINFO, "/etc/nginx/ssl/ca.cert.pem");

    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2L);  // 2초 타임아웃 등 필요시

    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        curl_easy_cleanup(curl);
        throw std::runtime_error("curl_easy_perform() failed in stop-status: " + std::string(curl_easy_strerror(res)));
    }
    curl_easy_cleanup(curl);

    // JSON 파싱
    try {
        auto j = json::parse(readBuffer);
        StopStatusData result;
        result.station_id = j.at("station_id").get<std::string>();
        result.updated_at = j.at("updated_at").get<std::string>();
        result.platform_status = j.at("platform_status").get<std::vector<int>>();
        return result;
    } catch (json::parse_error& e) {
        throw std::runtime_error("JSON parse error: " + std::string(e.what()));
    }
}