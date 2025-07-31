// Just for test(making shm with JSON data)
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#if 1
#include "json.hpp"
#endif

using json = nlohmann::json;

#define SHM_SIZE 4096  // JSON 데이터를 위한 충분한 크기

int main() {
  const char* shm_name = "/bus_approach";
  const size_t shm_size = SHM_SIZE;

  // 공유 메모리 생성 또는 열기 - 모든 사용자 접근 가능
  int fd = shm_open(shm_name, O_CREAT | O_RDWR, 0777);
  if (fd == -1) {
    std::cerr << "shm_open failed: " << strerror(errno) << std::endl;
    return 1;
  }

  // 공유 메모리 크기 설정
  if (ftruncate(fd, shm_size) == -1) {
    std::cerr << "ftruncate failed: " << strerror(errno) << std::endl;
    close(fd);
    return 1;
  }

  // 메모리 매핑
  void* addr = mmap(nullptr, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (addr == MAP_FAILED) {
    std::cerr << "mmap failed: " << strerror(errno) << std::endl;
    close(fd);
    return 1;
  }

  // JSON 데이터 생성
  json test_data = json::array();
  
  // 예시 번호판 데이터 (중복 포함)
  std::vector<std::string> plates = {
    "74사5815", 
    "75사1212", 
    "75사2650", 
    "74사1234"
  };

  // JSON 배열에 번호판 추가
  for (const auto& plate : plates) {
    test_data.push_back({{"busNumber", plate}});
  }

  // JSON을 문자열로 변환
  std::string json_str = test_data.dump();
  std::cout << "Writing JSON to shared memory: " << json_str << std::endl;

  // 공유 메모리에 JSON 문자열 쓰기
  char* shm_data = static_cast<char*>(addr);
  std::memset(shm_data, 0, shm_size);  // 초기화
  std::memcpy(shm_data, json_str.c_str(), json_str.length());

  std::cout << "Shared memory written with " << plates.size() << " bus plates in JSON format."
            << std::endl;

  munmap(addr, shm_size);
  close(fd);
  return 0;
}

/*
빌드 방법:
g++ -std=c++17 -Wall -o test_shm test_shm.cpp -lrt

사용 방법:
./test_shm    # JSON 형태의 테스트 데이터를 공유 메모리에 쓰기

공유 메모리 확인:
ls -la /dev/shm/ | grep bus

공유 메모리 삭제:
sudo rm -f /dev/shm/bus_approach

공유 메모리 권한 설정:
sudo chmod 777 /dev/shm/bus_approach
*/