#include "shm_reader.h"
#include "stop_status.hpp"
#include "../../common/bus_sequence.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <vector>
#include <string>

// HTTP + JSON
#include <curl/curl.h>
#include "json.hpp"

using json = nlohmann::json;

#define SHM_NAME_SEQUENCE "/busbom_approach"
#define MAX_BUSES 10

// 접근 버스 번호 시퀀스 읽기 (공유 메모리)
std::vector<std::string> readSequenceFromSHM() {
    std::vector<std::string> result;

    int fd = shm_open("/busbom_approach", O_RDONLY, 0666);
    if (fd == -1) {
        perror("shm_open failed");
        return result;
    }

    void* ptr = mmap(nullptr, sizeof(BusSequence), PROT_READ, MAP_SHARED, fd, 0);
    close(fd);
    if (ptr == MAP_FAILED) {
        perror("mmap failed");
        return result;
    }

    BusSequence* seq = static_cast<BusSequence*>(ptr);
    for (int i = 0; i < MAX_BUSES; ++i) {
        if (strlen(seq->plates[i]) == 0) break;
        result.emplace_back(seq->plates[i]);
    }

    //  디버그
    std::cout << "[DEBUG] Read sequence from SHM:\n";
    for (const auto& plate : result) {
        std::cout << "  - " << plate << "\n";
    }

    munmap(ptr, sizeof(BusSequence));
    return result;
}

// libcurl 콜백 함수: 응답 내용을 문자열로 저장
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

// CGI로부터 JSON 응답 받아 platform_status 벡터 파싱
std::vector<int> fetchStopStatusFromCGI() {
    std::vector<int> stop_status;
    CURL* curl = curl_easy_init();

    if (!curl) {
        std::cerr << "curl_easy_init() failed\n";
        return stop_status;
    }

    std::string readBuffer;
    curl_easy_setopt(curl, CURLOPT_URL, "http://192.168.0.82/cgi-bin/stop-status.cgi ");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2L); // 2초 타임아웃

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << "\n";
        curl_easy_cleanup(curl);
        return stop_status;
    }

    curl_easy_cleanup(curl);

    std::cout << "[DEBUG] HTTP response: " << readBuffer << std::endl;


    try {
        auto j = json::parse(readBuffer);
        for (int val : j["platform_status"]) {
            stop_status.push_back(val);
        }
    } catch (const std::exception& e) {
        std::cerr << "JSON parse error: " << e.what() << "\n";
        std::cerr << "Raw response: " << readBuffer << "\n";  // 추가 디버그
    }

    return stop_status;
}
