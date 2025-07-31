#include "display_writer.h"

/**
 * @brief 최종 배차/출차 지시사항을 드라이버가 원하는 형식으로 변환하여 출력.
 * @param result {플랫폼, 버스 ID 문자열} 쌍의 목록.
 * @param total_platforms 전체 유효 플랫폼 수.
 */
void printResultToStdout(const std::vector<std::pair<int, std::string>>& result, int total_platforms) {
    std::cout << "\n<<<<< 최종 지시사항 >>>>>\n";
    
    // 모든 플랫폼의 최종 상태를 만들기 위한 벡터
    std::vector<std::string> final_state(total_platforms, "");
    
    // result에 담긴 정보로 최종 상태 업데이트
    for (const auto& [plat, bus_str] : result) {
        if (plat < total_platforms) {
            final_state[plat] = bus_str;
        }
    }

    // 콜론으로 구분된 최종 문자열 생성
    std::string output_string = "";
    for (int i = 0; i < total_platforms; ++i) {
        if (final_state[i].empty()) {
            output_string += "\" \"";
        } else {
            output_string += "\"" + final_state[i] + "\"";
        }
        if (i < total_platforms - 1) {
            output_string += ":";
        }
    }
    
    std::cout << output_string << "\n";
    std::cout << "-------------------------------------\n";
}
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