#ifndef VIDEO_HPP
#define VIDEO_HPP

extern "C" { 
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libavutil/avutil.h>
}

#include <opencv2/opencv.hpp>
#include <iostream>

#include "metadata.hpp"


class VideoProcessor {
    private:
        AVCodecContext* codec_context_;
        const AVCodec* codec_;
        AVFrame* frame_;
        AVFrame* frame_rgb_;
        SwsContext* sws_context_;
        uint8_t* buffer_;
        bool initialized_;
        cv::Mat image_; // OpenCV image to hold the processed frame
        std::vector<cv::Mat> cropped_images_; // Store cropped images

        int width_;
        int height_;
        const double ORIGINAL_WIDTH = 3840.0;   // source resolution width
        const double ORIGINAL_HEIGHT = 2160.0;  // source resolution height

        void drawDetectionBoxes(cv::Mat& image, std::vector<Object>& objects);
        void cropDetectionBoxes(cv::Mat& image, std::vector<Object>& objects, cv::Point2f user_point = cv::Point2f(1920.0, 1080.0));
        bool process_packet(AVPacket* packet);
        bool process_frame(std::vector<Object>& objects);

    public:
        bool initialize(AVCodecParameters* codecpar);
        std::vector<cv::Mat> fetchFrames(AVPacket* pkt, std::vector<Object>& objects);
};


#endif // VIDEO_HPP