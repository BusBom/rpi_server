#include "display_writer.h"

using ordered_json = nlohmann::ordered_json;
using json = nlohmann::json;

// Shared memory constants (must match writer)
const char* SHM_SEQUENCE_NAME = "/busbom_sequence";
const size_t SHM_SEQUENCE_SIZE = 4096; // 4KB

/**
 * @brief 최종 배차/출차 지시사항을 클라이언트에게 전송(공유메모리에 출력)
 * @param result {플랫폼, 버스 ID 문자열} 쌍의 목록.
 * @param total_platforms 전체 유효 플랫폼 수.
 */
void printResultToSHM(const std::vector<std::pair<int, std::string>>& result, int total_platforms) {
    // 1. 모든 플랫폼의 최종 상태를 만들기 위한 벡터 생성
    std::vector<std::string> final_state(total_platforms);
    for (const auto& [plat, bus_str] : result) {
        if (plat < total_platforms) {
            final_state[plat] = bus_str;
        }
    }

    // 2. 콜론으로 구분된 최종 문자열 생성
    json sequence_data = json::array();
    for (int i = 0; i < total_platforms; ++i) {
        json item;
        item["platform"] = i + 1; // 플랫폼 번호를 1부터 시작하도록 설정
        if (final_state[i].empty()) {
            item["busNumber"] = " "; // 빈 플랫폼은 공백 한 칸으로 설정
        } else {
            item["busNumber"] = final_state[i];
        }
        sequence_data.push_back(item);
    }
    
    // 3. sequence.cgi가 원하는 최종 JSON 객체 형식으로 구성
    json final_response;
    final_response["parkingPoint"] = total_platforms;
    final_response["online"] = {0, 0, 0}; // 고정값
    final_response["sequence"] = sequence_data;

    // JSON 객체를 압축된 문자열로 변환
    std::string output_string = final_response.dump();

    std::cout << "\n<<<<< 최종 지시사항 (드라이버 전송) >>>>>\n";
    std::cout << output_string << "\n";
    std::cout << "-------------------------------------\n";

    // 3. 공유 메모리에 데이터 쓰기
    int fd = shm_open(SHM_SEQUENCE_NAME, O_CREAT | O_RDWR, 0666);
    if (fd == -1) {
        throw std::runtime_error("shm_open failed");
    }

    // 공유 메모리 파일의 크기를 지정
    if (ftruncate(fd, SHM_SEQUENCE_SIZE) == -1) {
        close(fd);
        throw std::runtime_error("ftruncate failed");
    }

    // 공유 메모리를 현재 프로세스의 주소 공간에 매핑
    void* addr = mmap(nullptr, SHM_SEQUENCE_SIZE, PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        close(fd);
        throw std::runtime_error("mmap failed");
    }
    
    // 매핑된 메모리에 최종 문자열 복사
    strncpy(static_cast<char*>(addr), output_string.c_str(), SHM_SEQUENCE_SIZE - 1);
    static_cast<char*>(addr)[SHM_SEQUENCE_SIZE - 1] = '\0';


    // 4. 자원 해제
    munmap(addr, SHM_SEQUENCE_SIZE);
    close(fd);
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