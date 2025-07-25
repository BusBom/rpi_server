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
#include "tf_ocr.hpp"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>


class HanwhaStreamer {
    private:
        MetadataParser metadataParser; // MetadataParser
        VideoProcessor videoProcessor; // VideoProcessor
        TFOCR tf_ocr;
        AVFormatContext *formatContext = nullptr;
        AVPacket pkt;
        AVDictionary *options = nullptr;
        std::string rtsp_url;
        int data_stream_index = -1;
        int video_stream_index = -1;
        bool is_initialized = false;

        const char * shm_name_;
        const size_t shm_size_;
        void* shm_ptr_;

        std::vector<CroppedObject> cropped_objects; // Store cropped objects for debugging
        
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

        // For multi-threading OCR
        std::thread ocr_thread_;
        std::queue<std::vector<CroppedObject>> ocr_queue_;
        std::mutex queue_mutex_;
        std::condition_variable queue_cond_;
        bool stop_ocr_thread_ = false;

        void ocr_worker();
        
    public:
        HanwhaStreamer();
        int initialize(const std::string& rtsp_url, const std::string& ocr_model_path, const std::string& ocr_labels_path);
        void run();
        ~HanwhaStreamer();
};

#endif // HANWHA_STREAMER_HPP