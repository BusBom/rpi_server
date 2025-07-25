#include "shm_reader.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <sstream>

#define SHM_NAME "/busbom_approach"
#define SHM_SIZE 4096

std::vector<int> readSequenceFromSHM() {
    /*
    std::vector<int> result;

    int fd = shm_open(SHM_NAME, O_RDONLY, 0666);  //공유 메모리를 열고 파일 디스크립터 얻음
    if (fd == -1) {
        perror("shm_open failed");
        return result;
    }

    void* ptr = mmap(nullptr, SHM_SIZE, PROT_READ, MAP_SHARED, fd, 0);
    close(fd);
    if (ptr == MAP_FAILED) {
        perror("mmap failed");
        return result;
    }

    std::string data(static_cast<char*>(ptr));
    munmap(ptr, SHM_SIZE);

    std::istringstream iss(data);
    std::string line;

    if (std::getline(iss, line)) {
        int count = std::stoi(line);
        for (int i = 0; i < count; ++i) {
            if (std::getline(iss, line)) {
                try {
                    result.push_back(std::stoi(line));
                } catch (...) {
                    std::cerr << "Invalid OCR text: " << line << std::endl;
                }
            }
        }
    }

    return result; */
    // 임시 mock 데이터 (테스트용)
    return {271, 2319, 273, 710};
}
