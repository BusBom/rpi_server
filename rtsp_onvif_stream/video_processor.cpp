#include "video_processor.hpp"
#include <iostream>
#include <mutex>

VideoProcessor::VideoProcessor(const std::string& window_name)
    : codec_context_(nullptr)
    , codec_(nullptr)
    , sws_context_(nullptr)
    , frame_(nullptr)
    , frame_rgb_(nullptr)
    , buffer_(nullptr)
    , width_(0)
    , height_(0)
    , initialized_(false)
    , dts_queue_() // Initialize dts_number as an empty queue 
{
}

VideoProcessor::~VideoProcessor() {
    cleanup();
}

bool VideoProcessor::initialize(AVCodecParameters* codecpar) {
    if (initialized_) {
        return true;
    }
    
    // Find the decoder for the video stream
    codec_ = avcodec_find_decoder(codecpar->codec_id);
    if (!codec_) {
        std::cerr << "Unsupported codec!" << std::endl;
        return false;
    }
    
    // Allocate codec context
    codec_context_ = avcodec_alloc_context3(codec_);
    if (!codec_context_) {
        std::cerr << "Could not allocate video codec context" << std::endl;
        return false;
    }
    
    // Copy codec parameters to context
    if (avcodec_parameters_to_context(codec_context_, codecpar) < 0) {
        std::cerr << "Could not copy codec parameters to context" << std::endl;
        return false;
    }
    
    // Open the codec
    if (avcodec_open2(codec_context_, codec_, nullptr) < 0) {
        std::cerr << "Could not open codec" << std::endl;
        return false;
    }
    
    width_ = codec_context_->width;
    height_ = codec_context_->height;
    
    // Allocate frames
    frame_ = av_frame_alloc();
    frame_rgb_ = av_frame_alloc();
    if (!frame_ || !frame_rgb_) {
        std::cerr << "Could not allocate frames" << std::endl;
        return false;
    }
    
    // Calculate buffer size and allocate buffer
    int num_bytes = av_image_get_buffer_size(AV_PIX_FMT_BGR24, width_, height_, 32);
    buffer_ = (uint8_t*)av_malloc(num_bytes * sizeof(uint8_t));
    
    // Assign buffer to frame
    av_image_fill_arrays(frame_rgb_->data, frame_rgb_->linesize, buffer_, 
                        AV_PIX_FMT_BGR24, width_, height_, 32);
    
    // Initialize software scaler context
    sws_context_ = sws_getContext(width_, height_, codec_context_->pix_fmt,
                                 width_, height_, AV_PIX_FMT_BGR24,
                                 SWS_BILINEAR, nullptr, nullptr, nullptr);
    
    if (!sws_context_) {
        std::cerr << "Could not initialize the conversion context" << std::endl;
        return false;
    }

    
    initialized_ = true;
    std::cout << "Video processor initialized successfully" << std::endl;
    std::cout << "Video resolution: " << width_ << "x" << height_ << std::endl;
    
    return true;
}

bool VideoProcessor::processPacket(AVPacket* packet, int64_t dts) {
    if (!initialized_) {
        std::cerr << "Video processor not initialized" << std::endl;
        return false;
    }
    
    // Always process the video packet to avoid freezing
    int ret = avcodec_send_packet(codec_context_, packet);
    if (ret < 0) {
        std::cerr << "Error sending packet to decoder" << std::endl;
        return false;
    }
    
    // Receive frame from decoder
    while (ret >= 0) {
        ret = avcodec_receive_frame(codec_context_, frame_);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            std::cerr << "Error during decoding" << std::endl;
            return false;
        }
        
        // process frame (with detection overlay if available)
        processFrame(frame_);
    }
    
    return true;

}

void VideoProcessor::setDetectionResults(const std::vector<Object>& objects, const int64_t dts) {
    std::lock_guard<std::mutex> lock(objects_mutex_);
    if (objects.empty()) {
        std::cout << "No detection results to update, clearing existing detections" << std::endl;
        detection_objects_queue_.push_back(std::vector<Object>());
        dts_queue_.push_back(dts);
        return;
    } else {
        detection_objects_queue_.push_back(objects);
        dts_queue_.push_back(dts); // Store the DTS number for reference
        return;
    }
}

void VideoProcessor::processFrame(AVFrame* frame) {
    // Convert frame to BGR format for OpenCV
    sws_scale(sws_context_, frame->data, frame->linesize, 0, height_,
              frame_rgb_->data, frame_rgb_->linesize);
    
    // Create OpenCV Mat from frame data
    cv::Mat image(height_, width_, CV_8UC3, frame_rgb_->data[0], frame_rgb_->linesize[0]);
    
    // Clone the image for processing
    processed_frame_ = image.clone();
    
    // Draw detection boxes on the processed frame (drawDetectionBoxes handles its own mutex)
    drawDetectionBoxes(processed_frame_);

    // Crop detection boxes (cropDetectionBoxes handles its own mutex)
    cropped_objects_ = cropDetectionBoxes(image);

}

std::vector<CroppedObject> VideoProcessor::cropDetectionBoxes(cv::Mat& image) {
    std::lock_guard<std::mutex> lock(objects_mutex_);

    // Clean up old detection objects (when queue is getting too large)
    const size_t MAX_QUEUE_SIZE = 10;
    while (detection_objects_queue_.size() > MAX_QUEUE_SIZE) {
        detection_objects_queue_.pop_front();
        if (!dts_queue_.empty()) {
            dts_queue_.pop_front();
        }
    }
    
    if (detection_objects_queue_.empty()) {
        return {}; // No detections to crop - exit early
    }
    
    // Use the most recent detection objects
    auto detection_objects_ = detection_objects_queue_.back();
    
    // Check if the most recent detection result is empty
    if (detection_objects_.empty()) {
        return {}; // Most recent result indicates no detections
    }
    
    std::vector<CroppedObject> cropped_objects;
    
    for (const auto& obj : detection_objects_) {
        // Scale bounding box coordinates to current resolution
        int x1 = static_cast<int>((obj.boundingBox.left / ORIGINAL_WIDTH) * width_);
        int y1 = static_cast<int>((obj.boundingBox.top / ORIGINAL_HEIGHT) * height_);
        int x2 = static_cast<int>((obj.boundingBox.right / ORIGINAL_WIDTH) * width_);
        int y2 = static_cast<int>((obj.boundingBox.bottom / ORIGINAL_HEIGHT) * height_);
        
        // Add 5px margin around the bounding box
        const int margin = 5;
        x1 = std::max(0, x1 - margin);
        y1 = std::max(0, y1 - margin);
        x2 = std::min(width_ - 1, x2 + margin);
        y2 = std::min(height_ - 1, y2 + margin);
        
        // Ensure valid rectangle dimensions
        if (x2 <= x1 || y2 <= y1) {
            continue; // Skip invalid rectangles
        }
        
        // Crop the object from the image
        cv::Rect crop_rect(x1, y1, x2 - x1, y2 - y1);
        cv::Mat cropped_image = image(crop_rect).clone();
        
        // Create CroppedObject with both image and metadata
        CroppedObject cropped_obj;
        cropped_obj.cropped_image = cropped_image;
        cropped_obj.object_info = obj;
        
        cropped_objects.push_back(cropped_obj);
    }
    
    return cropped_objects;
}

void VideoProcessor::drawDetectionBoxes(cv::Mat& image) {
    std::lock_guard<std::mutex> lock(objects_mutex_);
    
    // Clean up old detection objects (when queue is getting too large)
    const size_t MAX_QUEUE_SIZE = 10;
    while (detection_objects_queue_.size() > MAX_QUEUE_SIZE) {
        detection_objects_queue_.pop_front();
        if (!dts_queue_.empty()) {
            dts_queue_.pop_front();
        }
    }
    
    if (detection_objects_queue_.empty()) {
        return; // No detections - exit early
    }
    
    // Use the most recent detection objects (just peek)
    auto detection_objects_ = detection_objects_queue_.back();
    
    // Check if the most recent detection result is empty
    if (detection_objects_.empty()) {
        return; // Most recent result indicates no detections - don't draw anything
    }
    
    for (const auto& obj : detection_objects_) {
        // Scale bounding box coordinates to current resolution
        int x1 = static_cast<int>((obj.boundingBox.left / ORIGINAL_WIDTH) * width_);
        int y1 = static_cast<int>((obj.boundingBox.top / ORIGINAL_HEIGHT) * height_);
        int x2 = static_cast<int>((obj.boundingBox.right / ORIGINAL_WIDTH) * width_);
        int y2 = static_cast<int>((obj.boundingBox.bottom / ORIGINAL_HEIGHT) * height_);
        
        // Ensure coordinates are within image bounds
        x1 = std::max(0, std::min(x1, width_ - 1));
        y1 = std::max(0, std::min(y1, height_ - 1));
        x2 = std::max(0, std::min(x2, width_ - 1));
        y2 = std::max(0, std::min(y2, height_ - 1));
        
        // Draw bounding box
        cv::Scalar color(0, 255, 0); // Green color
        int thickness = 2;
        cv::rectangle(image, cv::Point(x1, y1), cv::Point(x2, y2), color, thickness);
        
        // Draw object information text
        std::string label = obj.typeName + " ID:" + std::to_string(obj.objectId) + 
                           " (" + std::to_string(static_cast<int>(obj.confidence * 100)) + "%)";
        
        int baseline = 0;
        cv::Size textSize = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseline);
        
        // Draw text background
        cv::rectangle(image, 
                     cv::Point(x1, y1 - textSize.height - 5),
                     cv::Point(x1 + textSize.width, y1),
                     color, -1);
        
        // Draw text
        cv::putText(image, label, cv::Point(x1, y1 - 5), 
                   cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 0), 1);
        
        // Draw center point (also needs scaling)
        int centerX = static_cast<int>((obj.centerOfGravity.x / ORIGINAL_WIDTH) * width_);
        int centerY = static_cast<int>((obj.centerOfGravity.y / ORIGINAL_HEIGHT) * height_);
        cv::circle(image, cv::Point(centerX, centerY), 3, cv::Scalar(0, 0, 255), -1); // Red dot
    }
}


void VideoProcessor::cleanup() {
    if (sws_context_) {
        sws_freeContext(sws_context_);
        sws_context_ = nullptr;
    }
    
    if (buffer_) {
        av_free(buffer_);
        buffer_ = nullptr;
    }
    
    if (frame_rgb_) {
        av_frame_free(&frame_rgb_);
    }
    
    if (frame_) {
        av_frame_free(&frame_);
    }
    
    if (codec_context_) {
        avcodec_free_context(&codec_context_);
    }
    
    initialized_ = false;
}
