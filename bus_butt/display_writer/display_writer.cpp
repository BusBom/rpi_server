#include "display_writer.h"

/**
 * @brief 최종 배차/출차 지시사항을 드라이버가 원하는 형식으로 변환하여 출력.
 * @param result {플랫폼, 버스 ID 문자열} 쌍의 목록.
 * @param total_platforms 전체 유효 플랫폼 수.
 */
void printResultToStdout(const std::vector<std::pair<int, std::string>>& result, int total_platforms) {
    std::cout << "\n<<<<< 최종 지시사항 >>>>>\n";
    
    // 모든 플랫폼의 최종 상태를 만들기 위한 벡터
    std::vector<std::string> final_state(total_platforms, "");
    
    // result에 담긴 정보로 최종 상태 업데이트
    for (const auto& [plat, bus_str] : result) {
        if (plat < total_platforms) {
            final_state[plat] = bus_str;
        }
    }

    // 콜론으로 구분된 최종 문자열 생성
    std::string output_string = "";
    for (int i = 0; i < total_platforms; ++i) {
        if (final_state[i].empty()) {
            output_string += "\" \"";
        } else {
            output_string += "\"" + final_state[i] + "\"";
        }
        if (i < total_platforms - 1) {
            output_string += ":";
        }
    }
    
    std::cout << output_string << "\n";
    std::cout << "-------------------------------------\n";
}
