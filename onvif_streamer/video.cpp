#include "video.hpp"

// intialize video processor with codec parameters
bool VideoProcessor::initialize(AVCodecParameters* codecpar) {
    if (initialized_) {
        return true;
    }

    codec_ = avcodec_find_decoder(codecpar->codec_id);
    if (!codec_) {
        std::cerr << "Unsupported codec!" << std::endl;
        return false;
    }

    codec_context_ = avcodec_alloc_context3(codec_);
    if (!codec_context_) {
        std::cerr << "Could not allocate video codec context" << std::endl;
        return false;
    }

    if (avcodec_parameters_to_context(codec_context_, codecpar) < 0) {
        std::cerr << "Could not copy codec parameters to context" << std::endl;
        return false;
    }

    if (avcodec_open2(codec_context_, codec_, nullptr) < 0) {
        std::cerr << "Could not open codec" << std::endl;
        return false;
    }

    width_ = codec_context_->width;
    height_ = codec_context_->height;

    frame_ = av_frame_alloc();
    frame_rgb_ = av_frame_alloc();
    if (!frame_ || !frame_rgb_) {
        std::cerr << "Could not allocate frames" << std::endl;
        return false;
    }

    int num_bytes = av_image_get_buffer_size(AV_PIX_FMT_BGR24, width_, height_, 32);
    buffer_ = (uint8_t*)av_malloc(num_bytes * sizeof(uint8_t));

    av_image_fill_arrays(frame_rgb_->data, frame_rgb_->linesize, buffer_, 
                         AV_PIX_FMT_BGR24, width_, height_, 32);

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

std::vector<cv::Mat> VideoProcessor::fetchFrames(AVPacket* pkt, std::vector<Object>& objects) {
    if (!initialized_) {
        std::cerr << "VideoProcessor not initialized" << std::endl;
        return cv::Mat();
    }

    if (!pkt) {
        std::cerr << "Null packet received" << std::endl;
        return cv::Mat();
    }

    // Process the packet to get the frame
    if (!process_packet(pkt)) {
        std::cerr << "Failed to process packet" << std::endl;
        return cv::Mat();
    }

    // Process the frame with detection boxes
    if (!process_frame(objects)) {
        std::cerr << "Failed to process frame" << std::endl;
        return cv::Mat();
    }
    
    return cropped_images_; // Return the processed image
}

// receive packet and process it to cv::Mat image_
bool VideoProcessor::process_packet(AVPacket* packet) {
    if (!initialized_) {
        std::cerr << "VideoProcessor not initialized" << std::endl;
        return false;
    }

    int ret = avcodec_send_packet(codec_context_, packet);
    if (ret < 0) {
        std::cerr << "Error sending packet to decoder" << std::endl;
        return false;
    }

    while (ret >= 0) {
        ret = avcodec_receive_frame(codec_context_, frame_);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            std::cerr << "Error during decoding" << std::endl;
            return false;
        }

        // Convert frame to BGR format for OpenCV
        sws_scale(sws_context_, frame_->data, frame_->linesize, 0, height_,
              frame_rgb_->data, frame_rgb_->linesize);
    
        // Create OpenCV Mat from frame data
        cv::Mat image(height_, width_, CV_8UC3, frame_rgb_->data[0], frame_rgb_->linesize[0]);
        image_ = image.clone(); // Store the processed frame
    }

    return true;
}

// fetch processed frame with detection boxes
bool VideoProcessor::process_frame(std::vector<Object>& objects) {
    if (!initialized_) {
        std::cerr << "VideoProcessor not initialized" << std::endl;
        return false;
    }
    
    // Draw detection boxes on the image
    cropDetectionBoxes(image_, objects);

    return true;
}
void VideoProcessor::cropDetectionBoxes(cv::Mat& image, std::vector<Object>& objects, cv::Point2f user_point) {
    
    std::vector<cv::Mat> cropped_images;
    for (const auto& obj : objects) {
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
        cropped_images.push_back(image(crop_rect));
    }

    cropped_images_ = cropped_images; // Store the cropped images
    
}



