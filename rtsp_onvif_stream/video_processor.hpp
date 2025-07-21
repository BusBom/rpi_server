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
#include <memory>
#include <queue>
#include <deque>

#include "metadata_parser.hpp"

struct CroppedObject {
            cv::Mat cropped_image;
            Object object_info; // Metadata about the object
            std::string ocr_text; // OCR text if available
        };

class VideoProcessor {
private:
    AVCodecContext* codec_context_;
    const AVCodec* codec_;
    SwsContext* sws_context_;
    AVFrame* frame_;
    AVFrame* frame_rgb_;
    uint8_t* buffer_;
    int width_;
    int height_;
    bool initialized_;

    const double ORIGINAL_WIDTH = 3840.0;   // source resolution width
    const double ORIGINAL_HEIGHT = 2160.0;  // source resolution height
    
    // cropped images
    std::vector<CroppedObject> cropped_objects_;
    
    // Detection results to draw
    std::deque< std::vector<Object> > detection_objects_queue_;
    std::mutex objects_mutex_;
    
    // DTS info
    std::deque<int64_t> dts_queue_;
    
public:
    VideoProcessor(const std::string& window_name = "RTSP Video Stream");
    ~VideoProcessor();
    
    bool initialize(AVCodecParameters* codecpar);
    bool processPacket(AVPacket* packet, int64_t dts);
    bool isInitialized() const { return initialized_; }
    void setDetectionResults(const std::vector<Object>& objects, const int64_t dts);
    
    // Get video dimensions
    int getWidth() const { return width_; }
    int getHeight() const { return height_; }

    // Get cropped objects (updated method)
    const std::vector<CroppedObject>& getCroppedObjects() const { return cropped_objects_; }
    
    // Get processed frame with detection boxes
    cv::Mat getProcessedFrame() const { return processed_frame_; }
    
    // Cleanup resources
    void cleanup();
    
private:
    // Convert AVFrame to OpenCV Mat and process with metadata
    void processFrame(AVFrame* frame);
    
    // Draw detection boxes
    void drawDetectionBoxes(cv::Mat& image);
    // Crop detection boxes
    std::vector<CroppedObject> cropDetectionBoxes(cv::Mat& image);
    
    // Store processed frame with detection boxes
    cv::Mat processed_frame_;
};

#endif // VIDEO_HPP
