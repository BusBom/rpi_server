#pragma once
#include <vector>
#include <utility>
#include <string>
#include <iostream>
#include <algorithm>
#include <fcntl.h>      // open()
#include <unistd.h>     // write(), close()
#include <cstring>      // strerror
#include <sys/stat.h>   // for mode constants

void printResultToStdout(const std::vector<std::pair<int, std::string>>& result, int total_platforms);
void writeResultToDevice(const std::vector<std::pair<int, std::string>>& result);