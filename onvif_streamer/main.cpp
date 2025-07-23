extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/error.h>
}

#include <unistd.h>
#include <iostream>
#include <algorithm>
#include <vector>
#include <deque>
#include <atomic>
#include <condition_variable>
#include <chrono>
#include <thread>
#include <mutex>

#include "metadata.hpp"
#include "video.hpp"

#define RTSP_URL "rtsp://192.168.0.64/profile2/media.smp"
#define MAX_PACKET_BUFFER 100
#define PTS_TOLERANCE 2000 // 100ms tolerance for PTS synchronization

AVFormatContext* formatContext = nullptr;
AVPacket pkt;
AVDictionary* options = nullptr;

std::deque<AVPacket*> v_buf_;                       // 비디오 패킷 버퍼
std::deque< OnvifMeta > d_buf_;           // 메타데이터 패킷 버퍼
std::mutex buf_mutex_;                              // 버퍼 접근 동기화 뮤텍스
std::condition_variable buf_cond_;                  // 버퍼 상태 알림용 CV
std::atomic<bool> stopSync{false};                  // 동기화 스레드 종료 플래그

int ret = -1;
int data_stream_index = -1;
int video_stream_index = -1;
const char* rtsp_url = RTSP_URL;

VideoProcessor videoProcessor;
MetadataParser metadataParser;

bool initialize() {
    /* ---- ---- initialize FFmpeg libraries ---- ---- */
    avformat_network_init();
    av_log_set_level(AV_LOG_VERBOSE); // Set log level for debugging

    // RTSP 연결 옵션 설정
    av_dict_set(&options, "rtsp_transport", "tcp", 0);
    av_dict_set(&options, "stimeout", "5000000", 0); // 5초 타임아웃
    av_dict_set(&options, "max_delay", "500000", 0); // 최대 지연 500ms

    if (options){
        AVDictionaryEntry* entry = nullptr;
        while ((entry = av_dict_get(options, "", entry, AV_DICT_IGNORE_SUFFIX))) {
            std::cout << "  " << entry->key << " = " << entry->value << std::endl;
        }
    } else {
        std::cout << "[HANWHA-ONVIF INIT] No options set (using defaults)" << std::endl;
    }

    ret = avformat_open_input(&formatContext, rtsp_url, nullptr, &options);
    if (options) {
        av_dict_free(&options);
    }

    if (ret < 0) {
        char error_buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, error_buf, sizeof(error_buf));
        fprintf(stderr, "Could not open input: %s\n", error_buf);
        fprintf(stderr, "Error code: %d\n", ret);
        return false;
    }

    ret = avformat_find_stream_info(formatContext, nullptr);
    if (ret < 0) {
        char error_buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, error_buf, sizeof(error_buf));
        fprintf(stderr, "Could not find stream info: %s\n", error_buf);
        avformat_close_input(&formatContext);
        return false; 
    }

    for (unsigned int i=0; i < formatContext->nb_streams; i++) {
        AVStream *stream = formatContext->streams[i];
        std::cout << "[HANWHA-ONVIF INIT] Stream " << i << ": codec_type=" << stream->codecpar->codec_type 
                  << ", codec_id=" << stream->codecpar->codec_id << std::endl;
        
        if (stream->codecpar->codec_type == AVMEDIA_TYPE_DATA) {
            data_stream_index = i;
            std::cout << "[HANWHA-ONVIF INIT] Found data stream at index: " << data_stream_index 
                      << " (codec_id: " << stream->codecpar->codec_id << ")" << std::endl;
        }
        else if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index = i;
            std::cout << "[HANWHA-ONVIF INIT] Found video stream at index: " << video_stream_index << std::endl;
        }
    }

    if (data_stream_index < 0) {
        fprintf(stderr, "No data stream found\n");
    }
    if (video_stream_index < 0) {
        fprintf(stderr, "No video stream found\n");
    }
    if (data_stream_index < 0 && video_stream_index < 0) {
        avformat_close_input(&formatContext);
        return false;
    }

    std::cout << "[HANWHA-ONVIF INIT] Final stream indices - Video: " << video_stream_index 
              << ", Data: " << data_stream_index << std::endl;

    if (!videoProcessor.initialize(formatContext->streams[video_stream_index]->codecpar)) {
        std::cerr << "[HANWHA-ONVIF INIT] Failed to initialize video processor" << std::endl;
        avformat_close_input(&formatContext);
        return false;
    }
    

    return true;
}

void sync_thread() {
    usleep(300000);
    std::cout << "[HANWHA-ONVIF SYNC] Synchronization thread started" << std::endl;
    
    auto last_data_time = std::chrono::steady_clock::now();
    const auto DATA_TIMEOUT = std::chrono::seconds(2); // 2초 타임아웃
    
    while(!stopSync) {
        std::unique_lock<std::mutex> lock(buf_mutex_);
        
        // 조건: 종료 신호이거나 비디오가 있고 (데이터가 있거나 타임아웃)
        buf_cond_.wait_for(lock, std::chrono::milliseconds(100), [] {
            return stopSync || !v_buf_.empty();
        });
        
        if (stopSync) break;
        
        auto current_time = std::chrono::steady_clock::now();
        bool data_timeout = (current_time - last_data_time) > DATA_TIMEOUT;
        
        while(!v_buf_.empty()) {
            bool has_data = !d_buf_.empty();
            
            if (has_data) {
                // 데이터가 있는 경우 - 기존 동기화 로직
                int64_t video_pts = v_buf_.front()->pts;
                int64_t data_pts = d_buf_.front().pts;
                int64_t diff = video_pts - data_pts;

                if (std::llabs(diff) <= PTS_TOLERANCE) {
                    // 동기화된 패킷 처리
                    AVPacket* vPkt = v_buf_.front(); v_buf_.pop_front();
                    OnvifMeta dMeta = d_buf_.front();  d_buf_.pop_front();
                    lock.unlock();

                    std::cout << "[HANWHA-ONVIF SYNC] Processing synced packets (Video PTS: " 
                              << video_pts << ", Data PTS: " << data_pts << ", Objects: " << dMeta.objects.size() << ")" << std::endl;

                    /* Synced Packet Process! */
                    std::vector<Object> result = dMeta.objects;
                    
                    try {
                        cv::Mat processed_frame = videoProcessor.fetchFrame(vPkt, result);
                        if (processed_frame.empty()) {
                            std::cerr << "[HANWHA-ONVIF SYNC] Warning: Empty frame returned" << std::endl;
                        } else {
                            std::cout << "[HANWHA-ONVIF SYNC] Frame processed successfully (" 
                                      << processed_frame.cols << "x" << processed_frame.rows << ")" << std::endl;
                        }
                    } catch (const std::exception& e) {
                        std::cerr << "[HANWHA-ONVIF SYNC] Exception in video processing: " << e.what() << std::endl;
                    }

                    av_packet_free(&vPkt);
                    last_data_time = current_time; // 데이터 처리 시간 업데이트
                    lock.lock();
                } else if (video_pts < data_pts) {
                    // 비디오 PTS가 더 작으면 비디오 드롭
                    std::cout << "[HANWHA-ONVIF SYNC] Dropping video packet (PTS: " << video_pts << ")" << std::endl;
                    av_packet_free(&v_buf_.front());
                    v_buf_.pop_front();
                } else {
                    // 데이터 PTS가 더 작으면 데이터 드롭
                    std::cout << "[HANWHA-ONVIF SYNC] Dropping data packet (PTS: " << data_pts << ")" << std::endl;
                    d_buf_.pop_front();
                    last_data_time = current_time;
                }
            } else if (data_timeout) {
                // 데이터가 없고 타임아웃된 경우 - 비디오만 처리
                AVPacket* vPkt = v_buf_.front(); v_buf_.pop_front();
                int64_t video_pts = vPkt->pts;
                lock.unlock();

                std::cout << "[HANWHA-ONVIF SYNC] Processing video-only packet (No metadata, PTS: " 
                          << video_pts << ")" << std::endl;

                /* Video-only Process! */
                std::vector<Object> empty_result; // 빈 객체 리스트
                
                try {
                    cv::Mat processed_frame = videoProcessor.fetchFrame(vPkt, empty_result);
                    if (processed_frame.empty()) {
                        std::cerr << "[HANWHA-ONVIF SYNC] Warning: Empty frame returned (video-only)" << std::endl;
                    } else {
                        std::cout << "[HANWHA-ONVIF SYNC] Video-only frame processed successfully (" 
                                  << processed_frame.cols << "x" << processed_frame.rows << ")" << std::endl;
                    }
                } catch (const std::exception& e) {
                    std::cerr << "[HANWHA-ONVIF SYNC] Exception in video-only processing: " << e.what() << std::endl;
                }

                av_packet_free(&vPkt);
                lock.lock();
            } else {
                // 데이터를 기다림
                break;
            }
        }
    }
    std::cout << "[HANWHA-ONVIF SYNC] Synchronization thread terminated" << std::endl;
}



int main() {

    if (!initialize()) {
        std::cerr << "Failed to initialize RTSP stream" << std::endl;
        return -1;
    }

    std::thread syncThread(sync_thread); // Start synchronization thread

    // Main loop to read packets
    int read_ret;
    int packet_count = 0;  // 패킷 카운터 추가
    auto last_log_time = std::chrono::steady_clock::now();  // 로그 시간 추적
    
    while ((read_ret = av_read_frame(formatContext, &pkt)) >= 0) {
        packet_count++;
        
        // 10초마다 상태 로그 출력
        auto current_time = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(current_time - last_log_time).count() >= 10) {
            std::lock_guard<std::mutex> status_lock(buf_mutex_);
            std::cout << "[HANWHA-ONVIF MAIN] Processed " << packet_count << " packets. "
                      << "Video buffer: " << v_buf_.size() << ", Data buffer: " << d_buf_.size() << std::endl;
            last_log_time = current_time;
            packet_count = 0; // 카운터 리셋
        }
        
        {
            std::lock_guard<std::mutex> lock(buf_mutex_);
            
            // 패킷 스트림 인덱스 디버깅
            if (pkt.stream_index != data_stream_index && pkt.stream_index != video_stream_index) {
                std::cout << "[HANWHA-ONVIF MAIN] Unknown stream index: " << pkt.stream_index 
                          << " (video: " << video_stream_index << ", data: " << data_stream_index << ")" << std::endl;
            }
            
            if (pkt.stream_index == data_stream_index) {
                // Process data packet - 올바른 버퍼에 저장
                std::cout << "Data Packet PTS: " << pkt.pts << ", DTS: " << pkt.dts << std::endl;
                AVPacket* data_pkt = av_packet_alloc();
                if (data_pkt) {
                    av_packet_ref(data_pkt, &pkt); // 패킷 복사
                    d_buf_.push_back(metadataParser.fetchMetadata(data_pkt)); // 데이터 패킷을 데이터 버퍼에
                    av_packet_free(&data_pkt);  // 원본 패킷 해제 (OnvifMeta에 복사됨)
                    if (d_buf_.size() > MAX_PACKET_BUFFER) {
                        // Limit buffer size to prevent memory overflow
                        d_buf_.pop_front();  // OnvifMeta는 자동으로 소멸됨
                    }
                }
            }
            else if (pkt.stream_index == video_stream_index) {
                // Process video packet - 올바른 버퍼에 저장
                std::cout << "Video Packet PTS: " << pkt.pts << ", DTS: " << pkt.dts << std::endl;
                AVPacket* video_pkt = av_packet_alloc();
                if (video_pkt) {
                    av_packet_ref(video_pkt, &pkt); // 패킷 복사
                    v_buf_.push_back(video_pkt); // 비디오 패킷을 비디오 버퍼에
                    if (v_buf_.size() > MAX_PACKET_BUFFER) {
                        // Limit buffer size to prevent memory overflow
                        av_packet_free(&v_buf_.front());
                        v_buf_.pop_front();
                    }
                }
            }
        }
        buf_cond_.notify_one(); // Notify any waiting threads that a new packet is available
        av_packet_unref(&pkt); // Free the packet after processing  
    } // while (av_read_frame)

    if (read_ret < 0) {
        char error_buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(read_ret, error_buf, sizeof(error_buf));
        std::cerr << "av_read_frame failed: " << error_buf << " (code: " << read_ret << ")" << std::endl;
    }

    stopSync = true;                         // 동기화 스레드 종료 플래그 설정
    buf_cond_.notify_all();                  // 대기 중이면 깨우기
    syncThread.join();                       // 스레드 종료 대기

    {
        std::lock_guard<std::mutex> lock(buf_mutex_);
        while (!v_buf_.empty()) {
            av_packet_free(&v_buf_.front());
            v_buf_.pop_front();
        }
        while (!d_buf_.empty()) {
            d_buf_.pop_front();  // OnvifMeta는 자동으로 소멸됨
        }
    }

    avformat_close_input(&formatContext);    // 포맷 컨텍스트 정리
    avformat_network_deinit();               // FFmpeg 네트워크 해제
    
}