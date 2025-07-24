#include "butt_matcher.h"
std::vector<std::pair<int, int>> matchBusToPlatforms(
    const std::vector<int>& sequence,
    const std::vector<int>& stop_status)
{
    std::vector<std::pair<int, int>> result;
    int seqIdx = 0;
    for (int platIdx = 0; platIdx < stop_status.size(); ++platIdx) {
        if (stop_status[platIdx] == 1) break;  // 정차 중이면 막힘
        if (seqIdx < sequence.size()) {
            result.emplace_back(platIdx, sequence[seqIdx]);
            seqIdx++;
        }
    }
    return result;
}