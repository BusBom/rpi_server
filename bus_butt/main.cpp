#include "shm_reader.h"
#include "mock_status.h"
#include "butt_matcher.h"
#include "display_writer.h"
#include <unistd.h>  // for sleep

int main() {
    while (true) {
        // 1. SHM에서 sequence 읽기
        std::vector<int> sequence = readSequenceFromSHM();

        // 2. stop-status (임시 벡터)
        std::vector<int> stopStatus = readStopStatusMock();

        // 3. 버스 → 플랫폼 매칭
        std::vector<std::pair<int, int>> matches = matchBusToPlatforms(sequence, stopStatus);

        // 4. 출력 (임시: stdout)
        printResultToStdout(matches);

        sleep(1); // 1초마다 반복
    }
    return 0;
}
