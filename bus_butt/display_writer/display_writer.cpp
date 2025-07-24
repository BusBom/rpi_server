#include "display_writer.h"
#include <iostream>

//지금은 /dev/bus_display 대신 콘솔로 출력
void printResultToStdout(const std::vector<std::pair<int, int>>& result) {
    std::cout << "\n[Matched Bus → Platform]\n";
    for (const auto& [plat, bus] : result) {
        std::cout << "P" << (plat + 1) << " ← Bus " << bus << "\n";
    }
    std::cout << "--------------------------\n";
}
