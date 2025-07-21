// OpenCV와 FCGI 충돌 방지
#define _FILE_OFFSET_BITS 64
#include <opencv2/opencv.hpp>

// ----- System Programming Header -----
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>  // for ntohl
#include <cstring>      // for strncpy, memset

// ----- Shared Memory Header -----
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

// ----- FCGI Header -----
#include <fcgi_stdio.h>
#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>
#include <sstream>

// ----- Shared Memory Setting -----
const char* SHM_NAME = "/busbom_frame";
#define FRAME_WIDTH  1280
#define FRAME_HEIGHT 720
#define FRAME_CHANNELS 3
#define FRAME_SIZE (FRAME_WIDTH * FRAME_HEIGHT * FRAME_CHANNELS)

/**
 * @brief 공유메모리에서 RAW 이미지를 읽어서 JPEG로 인코딩하는 함수
 * @return 인코딩된 JPEG 이미지 데이터. 실패 시 빈 벡터 반환.
 */
std::vector<char> capture_image_from_shm() {
    int shm_fd = -1;
    void* shm_ptr = nullptr;
    std::vector<char> jpeg_data;

    try {
        // 1. 공유메모리 열기
        shm_fd = shm_open(SHM_NAME, O_RDONLY, 0);
        if (shm_fd == -1) {
            FCGI_fprintf(FCGI_stderr, "[CGI ERROR] shm_open() failed: %s\n", strerror(errno));
            return {};
        }

        // 2. 공유메모리 크기 확인
        struct stat shm_stat;
        if (fstat(shm_fd, &shm_stat) == -1) {
            FCGI_fprintf(FCGI_stderr, "[CGI ERROR] fstat() failed: %s\n", strerror(errno));
            close(shm_fd);
            return {};
        }

        size_t shm_size = shm_stat.st_size;
        if (shm_size != FRAME_SIZE) {
            FCGI_fprintf(FCGI_stderr, "[CGI ERROR] Shared memory size mismatch. Expected: %d, Actual: %zu\n", FRAME_SIZE, shm_size);
            close(shm_fd);
            return {};
        }

        // 3. 공유메모리 매핑
        shm_ptr = mmap(nullptr, shm_size, PROT_READ, MAP_SHARED, shm_fd, 0);
        if (shm_ptr == MAP_FAILED) {
            FCGI_fprintf(FCGI_stderr, "[CGI ERROR] mmap() failed: %s\n", strerror(errno));
            close(shm_fd);
            return {};
        }

                // 4. RAW BGR 이미지 데이터를 JPEG로 인코딩
        unsigned char* raw_data = static_cast<unsigned char*>(shm_ptr);
        
        // BGR 형식의 RAW 데이터를 OpenCV Mat으로 생성
        cv::Mat frame(FRAME_HEIGHT, FRAME_WIDTH, CV_8UC3, raw_data);
        
        // JPEG 인코딩
        std::vector<uchar> jpeg_buffer;
        std::vector<int> encode_params = { cv::IMWRITE_JPEG_QUALITY, 85 };
        
        if (!cv::imencode(".jpg", frame, jpeg_buffer, encode_params)) {
            FCGI_fprintf(FCGI_stderr, "[CGI ERROR] Failed to encode image to JPEG\n");
            munmap(shm_ptr, shm_size);
            close(shm_fd);
            return {};
        }
        
        // JPEG 데이터를 char vector로 변환
        jpeg_data.resize(jpeg_buffer.size());
        memcpy(jpeg_data.data(), jpeg_buffer.data(), jpeg_buffer.size());

        // 5. 정리
        munmap(shm_ptr, shm_size);
        close(shm_fd);

    } catch (std::exception& e) {
        FCGI_fprintf(FCGI_stderr, "[CGI ERROR] Exception in capture_image_from_shm: %s\n", e.what());
        if (shm_ptr && shm_ptr != MAP_FAILED) {
            munmap(shm_ptr, FRAME_SIZE);
        }
        if (shm_fd != -1) {
            close(shm_fd);
        }
        return {};
    }

    return jpeg_data;
}

int main() {
    while (FCGI_Accept() >= 0) {
        // GET 요청만 처리
        char* request_method = getenv("REQUEST_METHOD");
        if (!request_method || strcmp(request_method, "GET") != 0) {
            printf("Content-Type: application/json\r\n\r\n");
            printf("{\"result\":\"error\",\"msg\":\"Only GET method is supported.\"}\n");
            continue;
        }

        // 공유메모리에서 이미지 캡쳐
        std::vector<char> image_jpeg = capture_image_from_shm();
        
        if (!image_jpeg.empty()) {
            // 성공 시: JPEG 이미지 출력
            printf("Content-Type: image/jpeg\r\n");
            printf("Content-Length: %zu\r\n", image_jpeg.size());
            printf("Cache-Control: no-cache, no-store, must-revalidate\r\n");
            printf("Pragma: no-cache\r\n");
            printf("Expires: 0\r\n\r\n");
            FCGI_fflush(FCGI_stdout);
            FCGI_fwrite(image_jpeg.data(), 1, image_jpeg.size(), FCGI_stdout);
            FCGI_fflush(FCGI_stdout);
        } else {
            // 실패 시: 오류 메시지 JSON 출력
            printf("Content-Type: application/json\r\n\r\n");
            printf("{\"result\":\"error\",\"msg\":\"Failed to capture image from shared memory.\"}\n");
        }
    }
    return 0;
}