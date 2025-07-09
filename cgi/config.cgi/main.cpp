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

using ordered_json = nlohmann::ordered_json;

// Shared memory constants
const char* SHM_NAME = "/camera_config";
const size_t SHM_SIZE = 4096; // 4KB

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

                // Write to shared memory
                int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
                if (shm_fd == -1) {
                    throw std::runtime_error("Failed to open shared memory");
                }
                if (ftruncate(shm_fd, SHM_SIZE) == -1) {
                    close(shm_fd);
                    throw std::runtime_error("Failed to set shared memory size");
                }
                char* shm_ptr = (char*)mmap(0, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
                if (shm_ptr == MAP_FAILED) {
                    close(shm_fd);
                    throw std::runtime_error("Failed to map shared memory");
                }

                std::string json_str = j.dump();
                strncpy(shm_ptr, json_str.c_str(), SHM_SIZE - 1);
                shm_ptr[SHM_SIZE - 1] = '\0'; // Ensure null termination

                munmap(shm_ptr, SHM_SIZE);
                close(shm_fd);

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
