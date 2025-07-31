// Just for test(making shm)
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <cstring>
#include <iostream>

#include "bus_sequence.hpp"

int main() {
  const char* shm_name = "/bus_approach";
  const size_t shm_size = sizeof(BusSequence);

  // 공유 메모리 생성 또는 열기
  int fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
  if (fd == -1) {
    std::cerr << "shm_open failed\n";
    return 1;
  }

  // 공유 메모리 크기 설정
  if (ftruncate(fd, shm_size) == -1) {
    std::cerr << "ftruncate failed\n";
    close(fd);
    return 1;
  }

  // 메모리 매핑
  void* addr = mmap(nullptr, shm_size, PROT_WRITE, MAP_SHARED, fd, 0);
  if (addr == MAP_FAILED) {
    std::cerr << "mmap failed\n";
    close(fd);
    return 1;
  }

  // 구조체 포인터로 캐스팅
  BusSequence* data = static_cast<BusSequence*>(addr);

  // 예시 번호판 데이터 입력
  const char* plates[] = {"74사5815", "75사1212", "75사2650", "서울99사9988"};
  int count = sizeof(plates) / sizeof(plates[0]);

  memset(data, 0, shm_size);  // 초기화
  for (int i = 0; i < count && i < MAX_BUSES; ++i) {
    strncpy(data->plates[i], plates[i], MAX_PLATE_LENGTH - 1);
  }

  std::cout << "Shared memory written with " << count << " bus plates."
            << std::endl;

  munmap(addr, shm_size);
  close(fd);
  return 0;
}

// g++ -std=c++17 -Wall -o test_shm test_shm.cpp -pthread -lrt
// ls /dev/shm
// sudo rm -rf /dev/shm/bus_approach