#include "hanwha_streamer.hpp"

HanwhaStreamer::HanwhaStreamer() 
    : frame_shm_fd_(-1), frame_shm_ptr_(nullptr), frame_shm_size_(0),
      detection_shm_fd_(-1), detection_shm_ptr_(nullptr), detection_shm_size_(0) {
}

int HanwhaStreamer::initialize(const std::string& rtsp_url) {
    this->rtsp_url = rtsp_url;

    
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
    return 0;
}

int HanwhaStreamer::run() {
    if (!is_initialized) {
        std::cerr << "HanwhaStreamer not initialized. Call initialize() first." << std::endl;
        return -1;
    }

    int packet_count = 0;
    int video_packet_count = 0;
    int data_packet_count = 0;
    
    auto start_time = std::chrono::steady_clock::now();

    while(av_read_frame(formatContext, &pkt) >= 0) {
        packet_count++;
        
        if (pkt.stream_index == data_stream_index) {
            data_packet_count++;
            std::cout << "Processing data packet #" << data_packet_count << " (size: " << pkt.size << " bytes)" << std::endl;
            std::cout << "Packet PTS: " << pkt.pts << ", DTS: " << pkt.dts << std::endl;
            metadataParser.processPacket(pkt.data, pkt.size); // Process the metadata packet
            std::vector<Object> result = metadataParser.getResults(); // Get the results from the parser
            
            // Send detection results to video processor
            if (videoProcessor.isInitialized() && !result.empty()) {
                // Only DTS is valid
                if (pkt.dts != AV_NOPTS_VALUE) {
                    videoProcessor.setDetectionResults(result, pkt.dts);
                    std::cout << "Updated video processor with " << result.size() << " detection results (DTS: " << pkt.dts << ")" << std::endl;
                } else {
                    std::cout << "Skipping detection results due to invalid DTS" << std::endl;
                }
            }
        }
        else if (pkt.stream_index == video_stream_index) {
            video_packet_count++;
            // Process video packet with OpenCV display
            if (videoProcessor.isInitialized()) {
                videoProcessor.processPacket(&pkt, pkt.dts);
                
                // Get processed frame and cropped objects
                cv::Mat processed_frame = videoProcessor.getProcessedFrame();
                std::vector<CroppedObject> cropped_objects = videoProcessor.getCroppedObjects();
                
                // Print cropped objects information to terminal
                std::cout << "Cropped objects count: " << cropped_objects.size() << std::endl;
                cv::imwrite("frame.jpg", processed_frame); // Save the processed frame for debugging
                std::cout << "frame.jpg'" << std::endl;
                cv::imwrite("cropped_objects.jpg", cropped_objects[0].cropped_image); // Save the cropped objects for debugging
                std::cout << "cropped.jpg'" << std::endl;
            }
        }
        
        av_packet_unref(&pkt); // Free the packet
    }
    
    metadataParser.processBuffer(); // Process any remaining buffered data
    avformat_close_input(&formatContext); // Close the input

    return 0; // Exit successfully
}

HanwhaStreamer::~HanwhaStreamer() {
    if (formatContext) {
        avformat_close_input(&formatContext);
    }
    if (options) {
        av_dict_free(&options);
    }
}