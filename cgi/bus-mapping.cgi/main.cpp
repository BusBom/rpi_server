#if 1
#include "json.hpp"
#endif

#include <fcgi_stdio.h>
#include <sys/stat.h>

#include <cstdlib>  // getenv
#include <cstring>  // strerror
#include <ctime>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>

using json = nlohmann::json;

// CSV 경로 상수
const std::string ROUTE_MAP_FILE = "/etc/bus/route_map.csv";

// 공유 자원
json approached_buses = json::array();
std::unordered_map<std::string, std::string> route_map;
time_t last_modified_time = 0;

// 동기화용 mutex
std::mutex bus_mutex;
std::mutex map_mutex;

// POST 요청 처리 함수
void handle_post_request(const std::string& body) {
  std::lock_guard<std::mutex> lock(bus_mutex);

  try {
    json input_array = json::parse(body);

    for (auto& item : input_array) {
      std::string bus_num = item.value("busNumber", "");
      auto it = route_map.find(bus_num);
      if (it != route_map.end()) {
        item["routeID"] = it->second;
      }
    }

    approached_buses = input_array;
  } catch (const std::exception& e) {
    std::cerr << "[ERROR] JSON parse failed: " << e.what() << std::endl;
    approached_buses = json::array();
  }
}

// CSV 파일에서 route_map 갱신
void update_route_map_from_file() {
  std::lock_guard<std::mutex> lock(map_mutex);

  struct stat file_stat;
  if (stat(ROUTE_MAP_FILE.c_str(), &file_stat) != 0) {
    std::cerr << "[WARN] Cannot stat route map: " << strerror(errno)
              << std::endl;
    return;
  }

  if (file_stat.st_mtime == last_modified_time) {
    return;  // 변경 없음
  }

  std::ifstream infile(ROUTE_MAP_FILE);
  if (!infile) {
    std::cerr << "[ERROR] Cannot open route map: " << ROUTE_MAP_FILE
              << std::endl;
    return;
  }

  std::unordered_map<std::string, std::string> new_map;
  std::string line;
  while (std::getline(infile, line)) {
    std::istringstream iss(line);
    std::string bus_num, route_id;
    if (!(iss >> bus_num >> route_id)) {
      continue;  // 무시
    }
    new_map[bus_num] = route_id;
  }

  route_map = std::move(new_map);
  last_modified_time = file_stat.st_mtime;
}

int main() {
  while (FCGI_Accept() >= 0) {
    update_route_map_from_file();

    char* method_cstr = getenv("REQUEST_METHOD");
    std::string method = method_cstr ? method_cstr : "";

    if (method == "POST") {
      char* content_length_str = getenv("CONTENT_LENGTH");
      int content_length = 0;

      if (content_length_str && strlen(content_length_str) > 0) {
        try {
          content_length = std::stoi(content_length_str);
        } catch (...) {
          content_length = 0;
        }
      }

      std::string post_body;
      if (content_length > 0) {
        post_body.resize(content_length);
        FCGI_fread(&post_body[0], 1, content_length, FCGI_stdin);
      }

      handle_post_request(post_body);

      std::string response;
      {
        std::lock_guard<std::mutex> lock(bus_mutex);
        response = approached_buses.dump(4);
      }

      printf("Content-Type: application/json\r\n");
      printf("Content-Length: %zu\r\n\r\n", response.size());
      printf("%s", response.c_str());
      FCGI_fflush(FCGI_stdout);

    } else if (method == "GET") {
      std::string response;
      {
        std::lock_guard<std::mutex> lock(bus_mutex);
        response = approached_buses.dump(4);
      }

      printf("Content-Type: application/json\r\n");
      printf("Content-Length: %zu\r\n\r\n", response.size());
      printf("%s", response.c_str());
      FCGI_fflush(FCGI_stdout);

    } else {
      json err = {{"error", "Unsupported HTTP method"}, {"method", method}};
      std::string err_str = err.dump(4);

      printf("Content-Type: application/json\r\n");
      printf("Content-Length: %zu\r\n\r\n", err_str.size());
      printf("%s", err_str.c_str());
      FCGI_fflush(FCGI_stdout);
    }
  }

  return EXIT_SUCCESS;
}
