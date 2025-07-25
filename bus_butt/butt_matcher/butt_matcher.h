#pragma once
#include <vector>
#include <string>

std::vector<std::pair<int, std::string>> matchBusToPlatforms(
    const std::vector<std::string>& sequence,
    const std::vector<int>& stop_status);
