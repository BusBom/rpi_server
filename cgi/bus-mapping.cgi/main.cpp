/**
  Caution : shared memory에 대한 쓰기 동작이 있어서, 읽기와 쓰기 권한이 있어야 함(chmod a+w /dev/shm/bus_approach)
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
#define SHM_SIZE 4096  // JSON 데이터를 위한 충분한 크기

const char* SHM_SEQUENCE_NAME = "/bus_approach";
const size_t SHM_SEQUENCE_SIZE = SHM_SIZE;

// 매핑 파일 경로 (시스템 경로)
const std::string ROUTE_MAP_FILE = "/etc/bus/route_map.csv"; //copy 필요


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
  if (ftruncate(fd, SHM_SEQUENCE_SIZE) == -1) {
    std::cerr << "[ERROR] ftruncate failed: " << strerror(errno) << std::endl;
    close(fd);
    return result;
  }

  void* addr = mmap(nullptr, SHM_SEQUENCE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  close(fd);

  if (addr == MAP_FAILED) {
    std::cerr << "[ERROR] mmap failed: " << strerror(errno) << std::endl;
    return result;
  }

  // 공유 메모리에서 JSON 문자열 읽기
  char* shm_data = static_cast<char*>(addr);
  
  // null 종료 문자 찾기
  std::string json_str(shm_data);
  size_t null_pos = json_str.find('\0');
  if (null_pos != std::string::npos) {
    json_str = json_str.substr(0, null_pos);
  }
  
  // 빈 문자열이면 빈 결과 반환
  if (json_str.empty() || json_str.find_first_not_of(" \t\n\r") == std::string::npos) {
    munmap(addr, SHM_SEQUENCE_SIZE);
    return result;
  }

  try {
    // JSON 파싱
    json shm_json = json::parse(json_str);
    
    // JSON 배열인지 확인
    if (!shm_json.is_array()) {
      std::cerr << "[ERROR] Shared memory data is not a JSON array" << std::endl;
      munmap(addr, SHM_SEQUENCE_SIZE);
      return result;
    }

    // 각 버스 번호판에 대해 routeID 매칭
    for (const auto& item : shm_json) {
      if (item.is_object() && item.contains("busNumber")) {
        std::string plate = item["busNumber"];
        
        if (!plate.empty()) {
          json result_item;
          result_item["busNumber"] = plate;

          auto it = route_map.find(plate);
          std::string route_id = (it != route_map.end()) ? it->second : "";
          result_item["routeID"] = route_id;

          // routeID가 있는 경우만 결과에 포함
          if (route_id.length() > 0) {
            result.push_back(result_item);
          }
        }
      }
    }

  } catch (const json::parse_error& e) {
    std::cerr << "[ERROR] JSON parse error: " << e.what() << std::endl;
    std::cerr << "[DEBUG] Raw data: " << json_str << std::endl;
  } catch (const std::exception& e) {
    std::cerr << "[ERROR] Exception while processing JSON: " << e.what() << std::endl;
  }

  // 공유 메모리 초기화 (빈 JSON 배열로)
  std::string empty_json = "[]";
  std::memset(shm_data, 0, SHM_SEQUENCE_SIZE);
  std::memcpy(shm_data, empty_json.c_str(), empty_json.length());

  munmap(addr, SHM_SEQUENCE_SIZE);
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
