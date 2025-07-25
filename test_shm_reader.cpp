#include <iostream>
#include <string>
#include <vector>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <sstream>

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
        std::istringstream iss(data);
        std::string line;
        
        // Read number of objects
        if (std::getline(iss, line)) {
            int count = std::stoi(line);
            std::cout << "Number of OCR texts: " << count << std::endl;
            
            // Read each OCR text
            for (int i = 0; i < count; i++) {
                if (std::getline(iss, line)) {
                    ocr_texts.push_back(line);
                    std::cout << "OCR[" << i << "]: " << line << std::endl;
                }
            }
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
    const char* shm_name = "/busbom_approach";
    size_t shm_size = 4096; // Adjust size as needed
    
    ShmReader reader(shm_name, shm_size);
    
    if (!reader.initialize()) {
        std::cerr << "Failed to initialize shared memory reader" << std::endl;
        return -1;
    }
    
    std::cout << "Press Enter to read shared memory (Ctrl+C to exit)..." << std::endl;
    
    while (true) {
        std::cin.get(); // Wait for Enter key
        
        std::cout << "\n--- Reading shared memory ---" << std::endl;
        std::vector<std::string> ocr_texts = reader.readOcrTexts();
        
        if (ocr_texts.empty()) {
            std::cout << "No OCR data found or shared memory is empty" << std::endl;
        } else {
            std::cout << "Successfully read " << ocr_texts.size() << " OCR texts" << std::endl;
        }
        std::cout << "--- End of reading ---\n" << std::endl;
    }
    
    return 0;
}
