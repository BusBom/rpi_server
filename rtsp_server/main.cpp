#include <fcntl.h>       // shm_open, O_* flags
#include <sys/mman.h>    // mmap
#include <sys/stat.h>    // mode constants
#include <unistd.h>      // ftruncate, close, write
#include <cstdio>        // FILE, popen
#include <cstdlib>       // exit
#include <cstring>       // memcpy
#include <iostream>

#define SHM_NAME   "/busbom_frame"
#define FRAME_WIDTH      1280 
#define FRAME_HEIGHT     720
#define FRAME_CHANNELS   3
#define FRAME_SIZE (FRAME_WIDTH * FRAME_HEIGHT * FRAME_CHANNELS)

int main () {
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666); // 공유 메모리 생성
    if (shm_fd == -1) {
        std::cerr << "Failed to open shared memory: " << strerror(errno) << std::endl;
        exit(EXIT_FAILURE);
    }
    
    void* shm_ptr = mmap(nullptr, FRAME_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_ptr == MAP_FAILED) {
        std::cerr << "Failed to map shared memory: " << strerror(errno) << std::endl;
        close(shm_fd);
        exit(EXIT_FAILURE); 
    }

    const char* ffmpeg_cmd =
        "ffmpeg -y -f rawvideo -pix_fmt bgr24 -s 1280x720 -r 15 -i - "
        "-c:v libx264 -preset ultrafast -tune zerolatency -f rtsp "
        "-rtsp_transport tcp -an rtsp://localhost:8554/stream";

    FILE* ffmpeg = popen(ffmpeg_cmd, "w");
    if (!ffmpeg) {
        std::cerr << "Failed to start FFmpeg pipeline" << std::endl;
        munmap(shm_ptr, FRAME_SIZE);
        close(shm_fd);
        exit(1);
    }

    uint8_t* framebuf = static_cast<uint8_t*>(shm_ptr);

    while (true) {
        // producer와 동기화 없이 프레임 계속 전송 (실전은 sync 권장)
        size_t written = fwrite(framebuf, 1, FRAME_SIZE, ffmpeg);
        if (written != FRAME_SIZE) {
            std::cerr << "Frame write error" << std::endl;
            break;
        }
        fflush(ffmpeg); // 버퍼 플러시(중요!)
        usleep(1000000 / 30); // 30fps
    }

    // 5. 종료 및 해제
    pclose(ffmpeg);
    munmap(shm_ptr, FRAME_SIZE);
    close(shm_fd);
    return 0;
}