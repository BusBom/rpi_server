#include "json.hpp"

#include <fcgi_stdio.h>
#include <iostream>
#include <stdio.h>
#include <string>
#include <sys/mman>                 // POSIX shared memory
#include <sys/stat>                 // For mode constants
#include <fcntl>                    // For O_CREAT, O_RDWR
#include <unistd>                   // For ftruncate, close, munmap
#include <string.h>                   // For strncpy

// UDS 통신용
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>

using ordered_json = nlohmann::ordered_json;

// 임시 UDS 함수 (주소 및 메시지 구조는 추후 논의)
void sendToUDS(const std::string& json_str) {
    // 구조만 잡아둠, 실제 동작은 추후 구현 예정
    // ex: send json_str via socket(AF_UNIX, SOCK_STREAM, 0)
}


int main() {
    while (FCGI_Accept() >= 0) {
        printf("Content-Type: application/json\r\n\r\n");

        char* content_length_str = getenv("CONTENT_LENGTH");
        int content_length = content_length_str ? atoi(content_length_str) : 0;

        if (content_length > 0) {
            char* post_data = new char[content_length + 1];
            FCGI_fread(post_data, 1, content_length, FCGI_stdin);
            post_data[content_length] = '\0';

            try {
                ordered_json j = json::parse(post_data);

                int brightness = 0, contrast = 0, exposure = 0, saturation = 0;
                bool enabled = true;
                std::string startTime = "01:00", endTime = "05:00";

                // 중첩 객체 접근 (profile)
                if (j.contains("camera")) {
                    json camera = j["camera"];
                    brightness = camera.value("brightness", 0);
                    contrast = camera.value("contrast", 0);
                    exposure = camera.value("exposure", 0);
                    saturation = camera.value("saturation", 0);
                }
                    
                if (j.contains("sleepMode")) {
                    json sleepMode = j["sleepMode"];
                    startTime = sleepMode.value("startTime", "01:00");
                    endTime = sleepMode.value("endTime", "05:00");
                }

                // 공유 메모리 → UDS 전송으로 변경
                std::string json_str = j.dump();
                sendToUDS(json_str);  // 실제 통신 구현은 추후

                ordered_json response = {
                    {"camera", {
                        {"brightness", brightness},
                        {"contrast", contrast},
                        {"exposure", exposure},
                        {"saturation", saturation}
                    }},
                    {"sleepMode", {
                        {"startTime", startTime},
                        {"endTime", endTime}
                    }}
                };
                std::cout << response.dump(4) << std::endl;
                
            } catch (std::exception& e) {
                json err = {
                    {"result", "error"},
                    {"msg", e.what()}
                };
                printf("%s\n", err.dump().c_str());
            }
            delete[] post_data;
        } else {
            printf("{\"result\":\"no_data\"}\n");
        }
    }
    return 0;
}
