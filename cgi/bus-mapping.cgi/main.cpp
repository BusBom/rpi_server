/**
  Caution : shared memory에 대한 쓰기 동작이 있어서, 읽기와 쓰기 권한이 있어야 함
 */

#if 1
#include "json.hpp"
#endif

#include <fcgi_stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstring>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>

using json = nlohmann::json;

#define MAX_BUSES 10
#define MAX_PLATE_LENGTH 24

const char* SHM_SEQUENCE_NAME = "/bus_approach";
const size_t SHM_SEQUENCE_SIZE = sizeof(char) * MAX_BUSES * MAX_PLATE_LENGTH;

// 매핑 파일 경로 (시스템 경로)
const std::string ROUTE_MAP_FILE = "/etc/bus/route_map.csv";

struct BusSequence {
  char plates[MAX_BUSES][MAX_PLATE_LENGTH];
};

std::unordered_map<std::string, std::string> route_map;
time_t last_modified_time = 0;
std::mutex map_mutex;

void update_route_map_from_file() {
  std::lock_guard<std::mutex> lock(map_mutex);

  struct stat file_stat;
  if (stat(ROUTE_MAP_FILE.c_str(), &file_stat) != 0) {
    std::cerr << "[WARN] Cannot stat route map: " << strerror(errno)
              << std::endl;
    return;
  }

  if (file_stat.st_mtime == last_modified_time) return;

  std::ifstream infile(ROUTE_MAP_FILE);
  if (!infile) {
    std::cerr << "[ERROR] Cannot open route map file.\n";
    return;
  }

  std::unordered_map<std::string, std::string> new_map;
  std::string bus_num, route_id;
  while (infile >> bus_num >> route_id) {
    new_map[bus_num] = route_id;
  }

  route_map = std::move(new_map);
  last_modified_time = file_stat.st_mtime;
}

json read_bus_sequence() {
  json result = json::array();

  // 공유 메모리 열기 (없으면 생성) - 모든 사용자 접근 가능
  int fd = shm_open(SHM_SEQUENCE_NAME, O_CREAT | O_RDWR, 0777);
  if (fd == -1) {
    std::cerr << "[ERROR] shm_open failed: " << strerror(errno) << std::endl;
    return result;
  }

  // 공유 메모리 크기 설정 (새로 생성된 경우)
  if (ftruncate(fd, sizeof(BusSequence)) == -1) {
    std::cerr << "[ERROR] ftruncate failed: " << strerror(errno) << std::endl;
    close(fd);
    return result;
  }

  void* addr = mmap(nullptr, sizeof(BusSequence), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  close(fd);

  if (addr == MAP_FAILED) {
    std::cerr << "[ERROR] mmap failed: " << strerror(errno) << std::endl;
    return result;
  }

  BusSequence* seq = reinterpret_cast<BusSequence*>(addr);

  for (int i = 0; i < MAX_BUSES; ++i) {
    std::string plate = seq->plates[i];
    
    // null 문자 제거
    size_t null_pos = plate.find('\0');
    if (null_pos != std::string::npos) {
      plate = plate.substr(0, null_pos);
    }
    
    if (plate.empty() || plate[0] == '\0') {
      continue;
    }

    json item;
    item["busNumber"] = plate;

    auto it = route_map.find(plate);
    std::string route_id = (it != route_map.end()) ? it->second : "";
    item["routeID"] = route_id;

    // routeID가 비어있어도 데이터를 포함
    if(route_id.length() > 0) {
      result.push_back(item);
    }
  }

  // 데이터 초기화 (다른 프로세스가 사용할 수 있도록)
  std::memset(seq, 0, sizeof(BusSequence));

  munmap(addr, sizeof(BusSequence));
  return result;
}

int main() {
  std::cerr << "[DEBUG] CGI application started" << std::endl;
  
  while (FCGI_Accept() >= 0) {
    std::cerr << "[DEBUG] Processing request" << std::endl;
    
    update_route_map_from_file();

    char* method_cstr = getenv("REQUEST_METHOD");
    std::string method = method_cstr ? method_cstr : "";

    std::cerr << "[DEBUG] Request method: " << method << std::endl;

    if (method == "GET") {
      std::cerr << "[DEBUG] Calling read_bus_sequence" << std::endl;
      json data = read_bus_sequence();
      std::string response = data.dump(4);

      std::cerr << "[DEBUG] Response: " << response << std::endl;

      printf("Content-Type: application/json\r\n");
      printf("Content-Length: %zu\r\n\r\n", response.size());
      printf("%s", response.c_str());
      FCGI_fflush(FCGI_stdout);
    } else {
      json err = {{"error", "Only GET is supported"}, {"method", method}};
      std::string err_str = err.dump(4);

      printf("Content-Type: application/json\r\n");
      printf("Content-Length: %zu\r\n\r\n", err_str.size());
      printf("%s", err_str.c_str());
      FCGI_fflush(FCGI_stdout);
    }
  }

  return 0;
}
