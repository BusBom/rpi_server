#include "butt_matcher.h"

std::vector<std::pair<int, std::string>> matchBusToPlatforms(
    const std::vector<std::string>& sequence,
    const std::vector<int>& stop_status)
{
    std::vector<std::pair<int, std::string>> result;
    int seqIdx = 0;

    for (int platIdx = 0; platIdx < stop_status.size(); ++platIdx) {
        if (stop_status[platIdx] == 1) break;
        if (seqIdx < sequence.size()) {
            result.emplace_back(platIdx, sequence[seqIdx]);
            seqIdx++;
        }
    }

    return result;
}
