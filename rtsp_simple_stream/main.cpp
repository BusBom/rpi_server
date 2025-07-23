#include <opencv2/opencv.hpp>      // OpenCV 주요 헤더
#include <thread>                  // std::thread 사용
#include <mutex>                   // std::mutex, std::unique_lock
#include <condition_variable>      // std::condition_variable
#include <deque>                   // std::deque 컨테이너
#include <fcntl.h>                 // shm_open
#include <sys/mman.h>              // mmap
#include <unistd.h>                // ftruncate, close
#include <sys/stat.h>              // fchmod, umask
#include <cstring>                 // std::memcpy
#include <iostream>                // std::cout, std::cerr
#include <csignal>                // std::signal
#include <atomic>                  // std::atomic

// ----- Shared Memory 설정 -----
#define SHM_NAME "/busbom_frame"
#define FRAME_WIDTH  1280
#define FRAME_HEIGHT 720
#define FRAME_CHANNELS 3
#define FRAME_SIZE (FRAME_WIDTH * FRAME_HEIGHT * FRAME_CHANNELS)

// 전역 변수
std::string rtsp_url = "rtsp://192.168.0.64:554/profile2/media.smp";
std::deque<cv::Mat> frame_queue;        // 캡처된 프레임을 저장하는 큐
std::mutex mtx;                          // 큐 접근 동기화용 뮤텍스
std::condition_variable cva;              // 큐 변동 알림용 조건변수
std::atomic<bool> running{true};         // 스레드 실행 제어 플래그
void* shm_ptr = nullptr;                 // Shared Memory 매핑 포인터

// 캡처 전용 스레드 함수
void capture_thread(const std::string& rtsp_url) {
    cv::VideoCapture cap(rtsp_url, cv::CAP_FFMPEG);  // FFmpeg 백엔드로 RTSP 열기
    if (!cap.isOpened()) {
        std::cerr << "RTSP 연결 실패" << std::endl; 
        return;
    }

    std::cout << "RTSP 연결 성공, 프레임 수신 중 ..." << std::endl;
    cap.set(cv::CAP_PROP_BUFFERSIZE, 1);             // 내부 버퍼 최소화
    cv::Mat frame;                                   // 프레임 저장용

    while (running.load()) {
        cap >> frame;                                // RTSP에서 한 프레임 읽기
        if (frame.empty()) continue;                 // 빈 프레임은 무시

        std::unique_lock<std::mutex> lk(mtx);        // 큐 접근 잠금
        if (frame_queue.size() >= 2)                 // 큐가 가득 차면
            frame_queue.pop_front();                 // 가장 오래된 프레임 제거
        frame_queue.push_back(frame.clone());        // 새 프레임 복사 후 삽입
        lk.unlock();                                 // 잠금 해제

        cva.notify_one();                             // 쓰기 스레드에 알림
    }
}

// Shared Memory 쓰기 전용 스레드 함수
void writer_thread(cv::VideoWriter* writer) {
    while (running.load()) {
        std::unique_lock<std::mutex> lk(mtx);        // 큐 접근 잠금
        cva.wait(lk, []{ return !frame_queue.empty() || !running.load(); });
                                                    // 큐에 데이터 또는 종료 신호 대기

        if (!frame_queue.empty()) {
            cv::Mat latest = frame_queue.back();     // 최신 프레임 취득
            frame_queue.clear();                     // 큐 비우기
            lk.unlock();                             // 잠금 해제

            std::memcpy(shm_ptr, latest.data, FRAME_SIZE);  // SHM에 바이트 복사
            // writer->write(latest);                   // VideoWriter로 파일에 기록
        } else {
            lk.unlock();                             // 프레임 없으면 잠금만 해제
        }
    }
}

int main() {
    // ----- 공유 메모리 생성 및 설정 -----
    umask(0); // 공유 메모리 이름이 이미 존재할 경우 제거
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        return 1;
    }

    // 크기 설정
    if (ftruncate(shm_fd, FRAME_SIZE) == -1) {
        perror("ftruncate");
        return 1;
    }

    if (fchmod(shm_fd, 0666) == -1) { 
        perror("fchmod"); /* 계속 진행 */
    }  

    // 메모리 매핑
    shm_ptr = mmap(0, FRAME_SIZE, PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_ptr == MAP_FAILED) {
        perror("mmap");
        return 1;
    }
    // VideoWriter 객체 생성
    std::string filename = "output.mp4";
    cv::VideoWriter writer(filename,
                           cv::VideoWriter::fourcc('m','p','4','v'),
                           10,
                           cv::Size(FRAME_WIDTH, FRAME_HEIGHT));

    if (!writer.isOpened()) {
        std::cerr << "VideoWriter 초기화 실패" << std::endl;
        return -1;
    }

    // 3) SIGINT(Ctrl+C) 핸들러 등록
    ::signal(SIGINT, [](int){
        running.store(false);                          // 실행 플래그 끔
        cva.notify_all();                               // 대기 중인 쓰레드 깨우기
    });

    std::thread t_cap(capture_thread, rtsp_url); // RTSP 캡처 스레드 시작
    std::thread t_write(writer_thread, &writer);           // Shared Memory 쓰기 스레드 시작

    std::cout << "프레임 캡처 및 공유 메모리 쓰기 스레드 시작" << std::endl;
    std::cout << "Ctrl+C 키를 누르면 종료합니다." << std::endl;

    t_cap.join();  // 캡처 스레드 종료 대기
    t_write.join(); // 쓰기 스레드 종료 대기
    

    // ✅ 여기서 저장 파일 경로를 지정
    

    // ✅ 디렉토리 존재 확인 및 생성
    // std::string out_dir = "/home/iam/videos";
    // struct stat st = {0};
    // if (stat(out_dir.c_str(), &st) == -1) {
    //     if (mkdir(out_dir.c_str(), 0777) == -1) {
    //         perror("mkdir");
    //         return -1;
    //     }
    // }

    // ----- 정리 -----
    munmap(shm_ptr, FRAME_SIZE);
    writer.release(); // VideoWriter 자원 해제
    close(shm_fd);
    shm_unlink(SHM_NAME); // 실제 운영 시에는 제거하지 않을 수도 있음

    return 0;
}
