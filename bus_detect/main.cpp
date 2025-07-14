#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <ncnn/net.h>                 // ncnn 네트워크 처리
#include <sys/mman.h>                 // POSIX shared memory
#include <sys/stat.h>                 // 파일 상태 상수
#include <fcntl.h>                    // shm_open 플래그
#include <unistd.h>                   // close(), usleep
#include <thread>                     // std::thread
#include <mutex>                      // std::mutex
#include <condition_variable>         // std::condition_variable
#include <queue>                      // std::queue
#include <vector>                     // std::vector
#include <iostream>                   // std::cout
#include <chrono>                     // std::chrono

#include "yolo.hpp"                   // YOLOv5Lite 클래스 정의


std::queue<cv::Mat> img_queue;        // 프레임 임시 저장 큐
std::mutex mtx;                       // 큐 보호용 뮤텍스
std::condition_variable cvn;          // 데이터 유무 통지용

const char* SHM_FRAME_NAME = "/busbom_frame";
constexpr int WIDTH = 1280;          
constexpr int HEIGHT = 720;          
constexpr int CH = 3;  

void reader_thread()
{
    int fd = shm_open(SHM_FRAME_NAME, O_RDONLY, 0666);                            //  SHM open
    if (fd == -1) {
        std::cerr << "Failed to open shared memory: " << strerror(errno) << std::endl;
        return;
    }
    void* ptr = mmap(nullptr, WIDTH * HEIGHT * CH, PROT_READ, MAP_SHARED, fd, 0);  //  mmap
    if (ptr == MAP_FAILED) {
        std::cerr << "Failed to map shared memory: " << strerror(errno) << std::endl;
        close(fd);
        return;
    }


    while (true)
    {
        // Origin Buffer to OpenCV Mat wrapping
        cv::Mat frame(HEIGHT, WIDTH, CV_8UC3, ptr);          
        cv::Mat copy = frame.clone();                      //  safe clone
        {   
            std::unique_lock<std::mutex> lock(mtx);
            if (img_queue.size() >= 2) img_queue.pop();   //  delete old queue frame if size exceeds 2
            img_queue.push(copy);                         //  insert new frame into queue
        }
        cvn.notify_one();                                 // wake up inference thread
        std::this_thread::sleep_for(std::chrono::milliseconds(5)); //  5ms sleep
    }

    munmap(ptr, WIDTH * HEIGHT * CH);    // unmapoing shared memory
    close(fd);                           // close fd

}


void inference_thread()
{
    Yolo yolo;
    yolo.load("vehicles_detection_best.ncnn.param", "vehicles_detection_best.ncnn.bin");  
    std::cout << "Model loaded successfully." << std::endl;

    while (true)
    {
        cv::Mat frame;
        std::vector<Object> objects;  
        {
            std::unique_lock<std::mutex> lock(mtx);
            cvn.wait(lock, []{ return !img_queue.empty(); });   // 데이터 올 때까지 대기
            frame = img_queue.front();                        
            img_queue.pop();                                    // 큐에서 제거
        }

        auto t1 = std::chrono::high_resolution_clock::now();
        yolo.detect(frame, objects, 640, 0.25f, 0.45f);                     // 추론 수행
        auto t2 = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
        std::cout << "[MAIN] yolo.detect() latency: " << ms << " ms" << std::endl;


        cv::Mat one_shot = yolo.draw_result(frame, objects);                // 결과 이미지에 그리기
        cv::imwrite("result.jpg", one_shot); // 결과 이미지 저장
    }
}

int main()
{
    std::cout << "Starting YOLOv5n Bus Detection..." << std::endl;
    std::thread t1(reader_thread);    // 프레임 읽기 스레드 시작
    std::thread t2(inference_thread); // 추론 스레드 시작
    t1.join();                        // 메인 스레드에서 대기
    t2.join();
    return 0;
}