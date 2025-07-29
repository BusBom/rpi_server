#include "display_writer.h"
#include <iostream>
#include <algorithm>
#include <fcntl.h>      // open()
#include <unistd.h>     // write(), close()
#include <cstring>      // strerror
#include <sys/stat.h>   // for mode constants

void printResultToStdout(const std::vector<std::pair<int, std::string>>& result) {
    if (result.empty()) return;  // 결과 없으면 출력하지 않음

    std::cout << "[Matched Bus → Platform]\n";
    for (const auto& [platform, plate_raw] : result) {
        std::string plate = plate_raw;
        plate.erase(std::remove(plate.begin(), plate.end(), '\n'), plate.end());
        if (plate.length() > 16) plate = plate.substr(0, 16);  // 너무 길면 자름
        std::cout << "P" << platform + 1 << " ← Bus " << plate << "\n";
    }
    std::cout << "--------------------------\n";
}

/*
// 매칭 결과를 /dev/serdev-uart 장치로 전송
void writeResultToDevice(const std::vector<std::pair<int, std::string>>& result) {
    // 플랫폼 수는 최대 4개로 고정 (P1~P4)
    std::vector<std::string> platform_bus(4, "");  // 초기값은 공백

    // result에서 platform 번호(P0~P3)에 해당하는 버스 번호 설정
    for (const auto& [platform, plate_raw] : result) {
        if (platform >= 0 && platform < 4) {
            std::string plate = plate_raw;
            plate.erase(std::remove(plate.begin(), plate.end(), '\n'), plate.end());
            platform_bus[platform] = plate;
        }
    }

    // 문자열 포맷: "BUS1:BUS2:BUS3:BUS4"
    std::string output;
    for (int i = 0; i < 4; ++i) {
        output += platform_bus[i];
        if (i < 3) output += ":";
    }

    // 디바이스 파일 열기
    int fd = open("/dev/serdev-uart", O_WRONLY);
    if (fd == -1) {
        std::cerr << "[ERROR] Failed to open /dev/serdev-uart: " << strerror(errno) << "\n";
        return;
    }

    // 문자열 쓰기
    ssize_t written = write(fd, output.c_str(), output.size());
    if (written < 0) {
        std::cerr << "[ERROR] Failed to write to /dev/serdev-uart: " << strerror(errno) << "\n";
    }

    close(fd);  // 파일 닫기
}
    */