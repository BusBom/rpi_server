#include "display_writer.h"
#include <iostream>
#include <algorithm>

void printResultToStdout(const std::vector<std::pair<int, std::string>>& result) {
    std::cout << "[Matched Bus → Platform]\n";
    for (const auto& [platform, plate_raw] : result) {
        std::string plate = plate_raw;
        plate.erase(std::remove(plate.begin(), plate.end(), '\n'), plate.end());

        std::cout << "P" << platform + 1 << " ← Bus " << plate << "\n";
    }
    std::cout << "--------------------------\n";
}
