#include "shm_reader.h"
#include "butt_matcher.h"
#include "display_writer.h"

#include <unistd.h>
#include <vector>
#include <iostream>

int main() {
    while (true) {
        std::vector<std::string> sequence = readSequenceFromSHM();
        std::vector<int> stopStatus = fetchStopStatusFromCGI();

        auto matches = matchBusToPlatforms(sequence, stopStatus);

        // 콘솔 출력 (디버깅용)
        printResultToStdout(matches);

        // UART 드라이버로 결과 전송
        writeResultToDevice(matches);

        sleep(1);
    }
    return 0;
}
