#include "hanwha_streamer.hpp"

HanwhaStreamer::HanwhaStreamer() 
    : frame_shm_fd_(-1), frame_shm_ptr_(nullptr), frame_shm_size_(0),
      detection_shm_fd_(-1), detection_shm_ptr_(nullptr), detection_shm_size_(0),
      output_format_context_(nullptr), output_stream_(nullptr), output_codec_context_(nullptr),
      output_codec_(nullptr), output_frame_(nullptr), output_sws_context_(nullptr),
      output_pts_(0), output_initialized_(false) {
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

int HanwhaStreamer::initializeOutput(const std::string& output_rtsp_url, int width, int height) {
    output_rtsp_url_ = output_rtsp_url;
    
    // Allocate output format context
    int ret = avformat_alloc_output_context2(&output_format_context_, nullptr, "rtsp", output_rtsp_url.c_str());
    if (ret < 0) {
        char error_buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, error_buf, sizeof(error_buf));
        std::cerr << "Could not allocate output format context: " << error_buf << std::endl;
        return -1;
    }
    
    // Find H.264 encoder
    output_codec_ = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!output_codec_) {
        std::cerr << "H.264 encoder not found" << std::endl;
        return -1;
    }
    
    // Create output stream
    output_stream_ = avformat_new_stream(output_format_context_, output_codec_);
    if (!output_stream_) {
        std::cerr << "Failed to create output stream" << std::endl;
        return -1;
    }
    
    // Allocate codec context
    output_codec_context_ = avcodec_alloc_context3(output_codec_);
    if (!output_codec_context_) {
        std::cerr << "Failed to allocate codec context" << std::endl;
        return -1;
    }
    
    // Set codec parameters
    output_codec_context_->codec_id = AV_CODEC_ID_H264;
    output_codec_context_->bit_rate = 2000000;
    output_codec_context_->width = width;
    output_codec_context_->height = height;
    output_codec_context_->time_base = {1, 25}; // 25 FPS
    output_codec_context_->framerate = {25, 1};
    output_codec_context_->gop_size = 50;
    output_codec_context_->max_b_frames = 1;
    output_codec_context_->pix_fmt = AV_PIX_FMT_YUV420P;
    
    // Set stream parameters
    output_stream_->time_base = output_codec_context_->time_base;
    
    // Apply global header flag if needed
    if (output_format_context_->oformat->flags & AVFMT_GLOBALHEADER) {
        output_codec_context_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }
    
    // Open codec
    ret = avcodec_open2(output_codec_context_, output_codec_, nullptr);
    if (ret < 0) {
        char error_buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, error_buf, sizeof(error_buf));
        std::cerr << "Could not open codec: " << error_buf << std::endl;
        return -1;
    }
    
    // Copy codec parameters to stream
    ret = avcodec_parameters_from_context(output_stream_->codecpar, output_codec_context_);
    if (ret < 0) {
        std::cerr << "Failed to copy codec parameters to stream" << std::endl;
        return -1;
    }
    
    // Allocate frame for output
    output_frame_ = av_frame_alloc();
    if (!output_frame_) {
        std::cerr << "Failed to allocate output frame" << std::endl;
        return -1;
    }
    
    output_frame_->format = output_codec_context_->pix_fmt;
    output_frame_->width = output_codec_context_->width;
    output_frame_->height = output_codec_context_->height;
    
    ret = av_frame_get_buffer(output_frame_, 0);
    if (ret < 0) {
        std::cerr << "Failed to allocate frame buffer" << std::endl;
        return -1;
    }
    
    // Initialize SWS context for color conversion
    output_sws_context_ = sws_getContext(
        width, height, AV_PIX_FMT_BGR24,
        output_codec_context_->width, output_codec_context_->height, AV_PIX_FMT_YUV420P,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );
    
    if (!output_sws_context_) {
        std::cerr << "Failed to initialize SWS context" << std::endl;
        return -1;
    }
    
    // Open output URL
    ret = avio_open(&output_format_context_->pb, output_rtsp_url.c_str(), AVIO_FLAG_WRITE);
    if (ret < 0) {
        char error_buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, error_buf, sizeof(error_buf));
        std::cerr << "Could not open output URL: " << error_buf << std::endl;
        return -1;
    }
    
    // Write header
    ret = avformat_write_header(output_format_context_, nullptr);
    if (ret < 0) {
        char error_buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, error_buf, sizeof(error_buf));
        std::cerr << "Could not write header: " << error_buf << std::endl;
        return -1;
    }
    
    output_initialized_ = true;
    std::cout << "RTSP output initialized successfully: " << output_rtsp_url << std::endl;
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
                
                // Send processed frame to RTSP output
                if (output_initialized_ && !processed_frame.empty()) {
                    sendFrameToRTSP(processed_frame);
                }
                
                // Print cropped objects information to terminal
                std::cout << "Cropped objects count: " << cropped_objects.size() << std::endl;
                cv::imwrite("frame.jpg", processed_frame); // Save the processed frame for debugging
                std::cout << "frame.jpg'" << std::endl;
                if (!cropped_objects.empty()) {
                    cv::imwrite("cropped_objects.jpg", cropped_objects[0].cropped_image); // Save the cropped objects for debugging
                    std::cout << "cropped.jpg'" << std::endl;
                }
            }
        }
        
        av_packet_unref(&pkt); // Free the packet
    }
    
    metadataParser.processBuffer(); // Process any remaining buffered data
    
    // Finalize RTSP output
    if (output_initialized_) {
        av_write_trailer(output_format_context_);
    }
    
    avformat_close_input(&formatContext); // Close the input

    return 0; // Exit successfully
}

int HanwhaStreamer::sendFrameToRTSP(const cv::Mat& frame) {
    if (!output_initialized_ || frame.empty()) {
        return -1;
    }
    
    // Convert OpenCV Mat (BGR) to AVFrame (YUV420P)
    const uint8_t* src_data[4] = {frame.data, nullptr, nullptr, nullptr};
    int src_linesize[4] = {static_cast<int>(frame.step[0]), 0, 0, 0};
    
    sws_scale(output_sws_context_, src_data, src_linesize, 0, frame.rows,
              output_frame_->data, output_frame_->linesize);
    
    // Set frame PTS
    output_frame_->pts = output_pts_++;
    
    // Encode frame
    int ret = avcodec_send_frame(output_codec_context_, output_frame_);
    if (ret < 0) {
        char error_buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, error_buf, sizeof(error_buf));
        std::cerr << "Error sending frame to encoder: " << error_buf << std::endl;
        return -1;
    }
    
    // Receive encoded packets
    AVPacket* pkt = av_packet_alloc();
    while (ret >= 0) {
        ret = avcodec_receive_packet(output_codec_context_, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            char error_buf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, error_buf, sizeof(error_buf));
            std::cerr << "Error receiving packet from encoder: " << error_buf << std::endl;
            av_packet_free(&pkt);
            return -1;
        }
        
        // Rescale packet timestamps
        av_packet_rescale_ts(pkt, output_codec_context_->time_base, output_stream_->time_base);
        pkt->stream_index = output_stream_->index;
        
        // Write packet to output
        ret = av_interleaved_write_frame(output_format_context_, pkt);
        if (ret < 0) {
            char error_buf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, error_buf, sizeof(error_buf));
            std::cerr << "Error writing packet: " << error_buf << std::endl;
            av_packet_free(&pkt);
            return -1;
        }
        
        av_packet_unref(pkt);
    }
    
    av_packet_free(&pkt);
    return 0;
}

HanwhaStreamer::~HanwhaStreamer() {
    if (formatContext) {
        avformat_close_input(&formatContext);
    }
    if (options) {
        av_dict_free(&options);
    }
    
    // Cleanup output resources
    if (output_sws_context_) {
        sws_freeContext(output_sws_context_);
    }
    if (output_frame_) {
        av_frame_free(&output_frame_);
    }
    if (output_codec_context_) {
        avcodec_free_context(&output_codec_context_);
    }
    if (output_format_context_) {
        if (output_format_context_->pb) {
            avio_closep(&output_format_context_->pb);
        }
        avformat_free_context(output_format_context_);
    }
}