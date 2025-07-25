#include "hanwha_streamer.hpp"
#include "tf_ocr.hpp"
#include <iostream>
#include <string>
#include <csignal>

static HanwhaStreamer* streamer = nullptr;

void signalHandler(int signal) {
    if (streamer) {
        delete streamer;
        streamer = nullptr;
    }
    std::cout << "\nProgram terminated by signal " << signal << std::endl;
    exit(0);
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <input_rtsp_url>" << std::endl;
        std::cerr << "Example: " << argv[0] << " rtsp://192.168.1.100:554/stream model.tflite labels.names" << std::endl;
        return -1;
    }

    std::string input_rtsp_url = argv[1];
    
    // Set up signal handlers for graceful shutdown
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    streamer = new HanwhaStreamer();
    
    std::cout << "Initializing Hanwha Streamer..." << std::endl;
    if (streamer->initialize(input_rtsp_url, "model.tflite", "labels.names") != 0) {
        std::cerr << "Failed to initialize Hanwha Streamer" << std::endl;
        delete streamer;
        return -1;
    }
    
    std::cout << "Press Ctrl+C to stop..." << std::endl;
    
    streamer->run();
    
    delete streamer;
    return 0;
}
