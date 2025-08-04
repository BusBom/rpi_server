#pragma once
#include <vector>
#include <utility>
#include <string>
#include <iostream>
#include <stdexcept>
#include <cstring> // for strcpy

// --- 외부 라이브러리 헤더 ---
#include <nlohmann/json.hpp>

// --- 공유 메모리 관련 헤더 ---
#include <fcntl.h>    // For O_* constants
#include <sys/mman.h> // For shared memory functions
#include <sys/stat.h> // For mode constants
#include <unistd.h>   // For ftruncate, close

void printResultToSHM(const std::vector<std::pair<int, std::string>>& result, int total_platforms);
void writeResultToDevice(const std::vector<std::pair<int, std::string>>& result, int total_platforms);