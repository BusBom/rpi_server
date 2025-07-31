/**
  Caution : shared memory에 대한 쓰기 동작이 있어서, 읽기와 쓰기 권한이 있어야 함(chmod a+w /dev/shm/bus_approach)
 */

#if 1
#include "json.hpp"
#endif

#include "../shared_memory_sync.hpp"
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

  // Initialize shared memory with synchronization
  SharedMemorySync shm_sync("/bus_approach", "/bus_approach_sem", 4096);
  if (!shm_sync.initialize()) {
    std::cerr << "[ERROR] Failed to initialize shared memory sync" << std::endl;
    return result;
  }

  // Use scoped lock for safe access
  {
    ScopedLock lock(shm_sync);
    if (!lock.is_locked()) {
      std::cerr << "[ERROR] Failed to acquire lock for shared memory" << std::endl;
      return result;
    }

    // Read JSON string from shared memory
    std::string json_str = shm_sync.read_data();
    std::cerr << "[DEBUG] Shared memory data: " << json_str << std::endl;
    
    // If empty, return empty result
    if (json_str.empty() || json_str.find_first_not_of(" \t\n\r") == std::string::npos) {
      return result;
    }

    try {
      // JSON 파싱
      json shm_json = json::parse(json_str);
      
      // JSON 배열인지 확인
      if (!shm_json.is_array()) {
        std::cerr << "[ERROR] Shared memory data is not a JSON array" << std::endl;
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
      std::cerr << "after routeID mapping" << std::endl;
      std::cerr << result.dump(4) << std::endl;

    } catch (const json::parse_error& e) {
      std::cerr << "[ERROR] JSON parse error: " << e.what() << std::endl;
      std::cerr << "[DEBUG] Raw data: " << json_str << std::endl;
    } catch (const std::exception& e) {
      std::cerr << "[ERROR] Exception while processing JSON: " << e.what() << std::endl;
    }

    // Clear shared memory after reading (reset to empty array)
    std::string empty_json = "[]";
    if (!shm_sync.write_data(empty_json)) {
      std::cerr << "[ERROR] Failed to clear shared memory" << std::endl;
    }
  } // ScopedLock automatically unlocks here

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
      std::cerr << data[0] << std::endl;
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
