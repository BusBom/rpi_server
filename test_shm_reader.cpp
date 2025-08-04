#include <iostream>
#include <string>
#include <vector>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <sstream>
#include "json.hpp"

using json = nlohmann::ordered_json;

class ShmReader {
private:
    const char* shm_name_;
    size_t shm_size_;
    void* shm_ptr_;
    
public:
    ShmReader(const char* name, size_t size) : 
        shm_name_(name), shm_size_(size), shm_ptr_(nullptr) {}
    
    bool initialize() {
        int shm_fd = shm_open(shm_name_, O_RDONLY, 0666);
        if (shm_fd == -1) {
            std::cerr << "Failed to open shared memory: " << strerror(errno) << std::endl;
            return false;
        }
        
        shm_ptr_ = mmap(nullptr, shm_size_, PROT_READ, MAP_SHARED, shm_fd, 0);
        close(shm_fd);
        
        if (shm_ptr_ == MAP_FAILED) {
            std::cerr << "Failed to map shared memory: " << strerror(errno) << std::endl;
            shm_ptr_ = nullptr;
            return false;
        }
        
        std::cout << "Connected to shared memory: " << shm_name_ << std::endl;
        return true;
    }
    
    std::vector<std::string> readOcrTexts() {
        std::vector<std::string> ocr_texts;
        
        if (shm_ptr_ == nullptr) {
            return ocr_texts;
        }
        
        std::string data(static_cast<char*>(shm_ptr_));
        
        // If data is empty, return empty vector
        if (data.empty()) {
            std::cout << "Shared memory is empty" << std::endl;
            return ocr_texts;
        }
        
        try {
            // Parse JSON data
            json plates_json = json::parse(data);
            
            if (!plates_json.is_array()) {
                std::cout << "Invalid JSON format: expected array" << std::endl;
                return ocr_texts;
            }
            
            std::cout << "Number of plates in JSON: " << plates_json.size() << std::endl;
            
            // Extract bus numbers from JSON
            for (size_t i = 0; i < plates_json.size(); i++) {
                const auto& plate = plates_json[i];
                if (plate.contains("busNumber") && plate["busNumber"].is_string()) {
                    std::string bus_number = plate["busNumber"].get<std::string>();
                    ocr_texts.push_back(bus_number);
                    std::cout << "Plate[" << i << "]: " << bus_number << std::endl;
                } else {
                    std::cout << "Plate[" << i << "]: Invalid format (missing busNumber)" << std::endl;
                }
            }
            
        } catch (const json::exception& e) {
            std::cerr << "Failed to parse JSON: " << e.what() << std::endl;
            std::cout << "Raw data: " << data << std::endl;
            return ocr_texts;
        }
        
        return ocr_texts;
    }
    
    ~ShmReader() {
        if (shm_ptr_ != nullptr) {
            munmap(shm_ptr_, shm_size_);
        }
    }
};

int main() {
    const char* shm_name = "/bus_approach";  // Updated to match main.cpp
    size_t shm_size = 4096; // Adjust size as needed
    
    ShmReader reader(shm_name, shm_size);
    
    if (!reader.initialize()) {
        std::cerr << "Failed to initialize shared memory reader" << std::endl;
        return -1;
    }
    
    std::cout << "Connected to shared memory: " << shm_name << std::endl;
    std::cout << "Press Enter to read JSON data from shared memory (Ctrl+C to exit)..." << std::endl;
    
    while (true) {
        std::cin.get(); // Wait for Enter key
        
        std::cout << "\n--- Reading JSON from shared memory ---" << std::endl;
        std::vector<std::string> ocr_texts = reader.readOcrTexts();
        
        if (ocr_texts.empty()) {
            std::cout << "No license plate data found or shared memory is empty" << std::endl;
        } else {
            std::cout << "Successfully read " << ocr_texts.size() << " license plates:" << std::endl;
            for (size_t i = 0; i < ocr_texts.size(); i++) {
                std::cout << "  " << (i+1) << ". " << ocr_texts[i] << std::endl;
            }
        }
        std::cout << "--- End of reading ---\n" << std::endl;
    }
    
    return 0;
}
