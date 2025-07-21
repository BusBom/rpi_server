#ifndef HANWHA_STREAMER_HPP
#define HANWHA_STREAMER_HPP

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/error.h>
}
#include <tinyxml2.h>
#include <iostream>
#include <vector>
#include <chrono>
#include <mutex>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>
#include <opencv2/opencv.hpp>

#include "metadata_parser.hpp"
#include "video_processor.hpp"


class HanwhaStreamer {
    private:
        MetadataParser metadataParser; // MetadataParser
        VideoProcessor videoProcessor; // VideoProcessor
        AVFormatContext *formatContext = nullptr;
        AVPacket pkt;
        AVDictionary *options = nullptr;
        std::string rtsp_url;
        int data_stream_index = -1;
        int video_stream_index = -1;
        bool is_initialized = false;
        
        // Shared memory for video frames
        int frame_shm_fd_ = -1;
        void* frame_shm_ptr_ = nullptr;
        size_t frame_shm_size_ = 0;
        
        // Shared memory for detection results
        int detection_shm_fd_ = -1;
        void* detection_shm_ptr_ = nullptr;
        size_t detection_shm_size_ = 0;
        
        // RTSP output components
        AVFormatContext* output_format_context_ = nullptr;
        AVStream* output_stream_ = nullptr;
        AVCodecContext* output_codec_context_ = nullptr;
        const AVCodec* output_codec_ = nullptr;
        AVFrame* output_frame_ = nullptr;
        SwsContext* output_sws_context_ = nullptr;
        int64_t output_pts_ = 0;
        bool output_initialized_ = false;
        std::string output_rtsp_url_;
        
    public:
        HanwhaStreamer();
        int initialize(const std::string& rtsp_url);
        int initializeOutput(const std::string& output_rtsp_url, int width, int height);
        int run();
        ~HanwhaStreamer();
        
    private:
        int sendFrameToRTSP(const cv::Mat& frame);
};

#endif // HANWHA_STREAMER_HPP