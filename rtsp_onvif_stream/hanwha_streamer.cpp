#include "hanwha_streamer.hpp"

HanwhaStreamer::HanwhaStreamer() 
    : frame_shm_fd_(-1), frame_shm_ptr_(nullptr), frame_shm_size_(0),
      detection_shm_fd_(-1), detection_shm_ptr_(nullptr), detection_shm_size_(0),
      output_format_context_(nullptr), output_stream_(nullptr), output_codec_context_(nullptr),
      output_codec_(nullptr), output_frame_(nullptr), output_sws_context_(nullptr),
      output_pts_(0), output_initialized_(false) {
}

void HanwhaStreamer::ocr_worker() {
    while (!stop_ocr_thread_) {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        queue_cond_.wait(lock, [this] { return !ocr_queue_.empty() || stop_ocr_thread_; });

        if (stop_ocr_thread_ && ocr_queue_.empty()) {
            return;
        }

        std::vector<CroppedObject> cropped_objects = std::move(ocr_queue_.front());
        ocr_queue_.pop();
        lock.unlock();

        std::cout << "Running OCR on cropped object..." << std::endl;

        for (auto& cropped_object : cropped_objects) {  
            std::string ocr_text = tf_ocr.run_ocr(cropped_object.cropped_image);
            cropped_object.ocr_text = ocr_text;
        }

    }
}

int HanwhaStreamer::initialize(const std::string& rtsp_url, const std::string& ocr_model_path, const std::string& ocr_labels_path) {
    this->rtsp_url = rtsp_url;
    tf_ocr.load_ocr(ocr_model_path, ocr_labels_path);
    
    avformat_network_init(); // Initialize
    int ret = -1;
    
    std::cout << "Connecting to RTSP stream: " << rtsp_url << std::endl;
    
    // set log level for debugging
    av_log_set_level(AV_LOG_VERBOSE);
    
    // options being set
    std::cout << "RTSP options configured:" << std::endl;
    if (options) {
        AVDictionaryEntry *entry = nullptr;
        while ((entry = av_dict_get(options, "", entry, AV_DICT_IGNORE_SUFFIX))) {
            std::cout << "  " << entry->key << " = " << entry->value << std::endl;
        }
    } else {
        std::cout << "  No options set (using defaults)" << std::endl;
    }
    
    ret = avformat_open_input(&formatContext, rtsp_url.c_str(), nullptr, &options);
    if (options) {
        av_dict_free(&options); 
    } 
    if (ret < 0) {
        char error_buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, error_buf, sizeof(error_buf));
        fprintf(stderr, "Could not open input: %s\n", error_buf);
        fprintf(stderr, "Error code: %d\n", ret);
        return -1;  
    }
    
    ret = avformat_find_stream_info(formatContext, nullptr);
    if (ret < 0) {        
        char error_buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, error_buf, sizeof(error_buf));
        fprintf(stderr, "Could not find stream info: %s\n", error_buf);
        avformat_close_input(&formatContext);
        return -1;
    }

    data_stream_index = -1;
    video_stream_index = -1;
    
    for (unsigned int i=0; i < formatContext->nb_streams; i++) {
        AVStream *stream = formatContext->streams[i];
        if (stream->codecpar->codec_type == AVMEDIA_TYPE_DATA) {
            data_stream_index = i;
            std::cout << "Found data stream at index: " << data_stream_index << std::endl;
        }
        else if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index = i;
            std::cout << "Found video stream at index: " << video_stream_index << std::endl;
            std::cout << "Video codec: " << avcodec_get_name(stream->codecpar->codec_id) << std::endl;
            std::cout << "Video resolution: " << stream->codecpar->width << "x" << stream->codecpar->height << std::endl;
            
            // Initialize video processor
            if (!videoProcessor.initialize(stream->codecpar)) {
                std::cerr << "Failed to initialize video processor" << std::endl;
            }
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
        return -1;
    }
    
    is_initialized = true;
    ocr_thread_ = std::thread(&HanwhaStreamer::ocr_worker, this);
    return 0;
}



void HanwhaStreamer::run() {
    if (!is_initialized) {
        std::cerr << "HanwhaStreamer not initialized. Call initialize() first." << std::endl;
        return;
    }

    int packet_count = 0;
    int video_packet_count = 0;
    int data_packet_count = 0;
    
    auto start_time = std::chrono::steady_clock::now();

    while(av_read_frame(formatContext, &pkt) >= 0) {
        packet_count++;
        
        if (pkt.stream_index == data_stream_index) {
            data_packet_count++;
    
            metadataParser.processPacket(pkt.data, pkt.size); // Process the metadata packet
            std::vector<Object> result = metadataParser.getResults(); // Get the results from the parser
            
            // Send detection results to video processor (including empty results to clear previous detections)
            if (videoProcessor.isInitialized()) {
                // Only DTS is valid
                if (pkt.dts != AV_NOPTS_VALUE) {
                    if (!result.empty()) {
                        // std::cout << "Received " << result.size() << " detection results (DTS: " << pkt.dts << ") Updated to video Processor" << std::endl;
                    } else {
                        // std::cout << "No detection results - clearing previous detections (DTS: " << pkt.dts << ")" << std::endl;
                    }
                    videoProcessor.setDetectionResults(result, pkt.dts);
                } else {
                    std::cout << "Skipping detection results due to invalid DTS" << std::endl;
                }
            }
        }
        else if (pkt.stream_index == video_stream_index) {
            video_packet_count++;
            // Process video packet with OpenCV display
            if (videoProcessor.isInitialized()) {
                // std::cout << "Video :: Packet PTS: " << pkt.pts << ", DTS: " << pkt.dts << std::endl;
                videoProcessor.processPacket(&pkt, pkt.dts);
                
                // Get processed frame and cropped objects
                cv::Mat processed_frame = videoProcessor.getProcessedFrame();
                std::vector<CroppedObject> cropped_objects = videoProcessor.getCroppedObjects();
                
                // Push cropped objects to the OCR queue
                {
                    std::lock_guard<std::mutex> lock(queue_mutex_);
                    ocr_queue_.push(cropped_objects);
                }
                queue_cond_.notify_one();
                
                // Print cropped objects information to terminal
                // cv::imwrite("frame.jpg", processed_frame); // Save the processed frame for debugging
                cv::imshow("Processed Frame", processed_frame);
                cv::waitKey(1); // Display the frame for a brief moment
            }
        }
        
        av_packet_unref(&pkt); // Free the packet
    }
    
    metadataParser.processBuffer(); // Process any remaining buffered data
    avformat_close_input(&formatContext); // Close the input
}



HanwhaStreamer::~HanwhaStreamer() {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        stop_ocr_thread_ = true;
    }
    queue_cond_.notify_one();
    if (ocr_thread_.joinable()) {
        ocr_thread_.join();
    }

    if (formatContext) {
        avformat_close_input(&formatContext);
    }
    if (options) {
        av_dict_free(&options);
    }
}
