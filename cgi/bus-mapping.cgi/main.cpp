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

// 매핑 파일 경로
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

  int fd = shm_open(SHM_SEQUENCE_NAME, O_RDONLY, 0);
  if (fd == -1) {
    std::cerr << "[ERROR] shm_open failed: " << strerror(errno) << std::endl;
    return result;
  }

  void* addr = mmap(nullptr, sizeof(BusSequence), PROT_READ, MAP_SHARED, fd, 0);
  close(fd);

  if (addr == MAP_FAILED) {
    std::cerr << "[ERROR] mmap failed: " << strerror(errno) << std::endl;
    return result;
  }

  BusSequence* seq = reinterpret_cast<BusSequence*>(addr);

  for (int i = 0; i < MAX_BUSES; ++i) {
    std::string plate = seq->plates[i];
    if (plate.empty() || plate[0] == '\0') continue;

    json item;
    item["busNumber"] = plate;

    auto it = route_map.find(plate);
    item["routeID"] = (it != route_map.end()) ? it->second : "";

    result.push_back(item);
  }

  munmap(addr, sizeof(BusSequence));
  return result;
}

int main() {
  while (FCGI_Accept() >= 0) {
    update_route_map_from_file();

    char* method_cstr = getenv("REQUEST_METHOD");
    std::string method = method_cstr ? method_cstr : "";

    if (method == "GET") {
      json data = read_bus_sequence();
      std::string response = data.dump(4);

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
