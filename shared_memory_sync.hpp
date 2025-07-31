#ifndef SHARED_MEMORY_SYNC_HPP
#define SHARED_MEMORY_SYNC_HPP

#include <semaphore.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <string>
#include <iostream>

class SharedMemorySync {
private:
    const char* shm_name_;
    const char* sem_name_;
    size_t shm_size_;
    void* shm_ptr_;
    sem_t* semaphore_;
    int shm_fd_;

public:
    SharedMemorySync(const char* shm_name, const char* sem_name, size_t shm_size) 
        : shm_name_(shm_name), sem_name_(sem_name), shm_size_(shm_size), 
          shm_ptr_(nullptr), semaphore_(nullptr), shm_fd_(-1) {}

    ~SharedMemorySync() {
        cleanup();
    }

    bool initialize() {
        // Create/open semaphore
        semaphore_ = sem_open(sem_name_, O_CREAT, 0666, 1);
        if (semaphore_ == SEM_FAILED) {
            std::cerr << "Failed to create/open semaphore: " << strerror(errno) << std::endl;
            return false;
        }

        // Create/open shared memory
        shm_fd_ = shm_open(shm_name_, O_CREAT | O_RDWR, 0777);
        if (shm_fd_ == -1) {
            std::cerr << "Failed to open shared memory: " << strerror(errno) << std::endl;
            sem_close(semaphore_);
            return false;
        }

        // Set size
        if (ftruncate(shm_fd_, shm_size_) == -1) {
            std::cerr << "Failed to set shared memory size: " << strerror(errno) << std::endl;
            close(shm_fd_);
            sem_close(semaphore_);
            return false;
        }

        // Map memory
        shm_ptr_ = mmap(nullptr, shm_size_, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd_, 0);
        if (shm_ptr_ == MAP_FAILED) {
            std::cerr << "Failed to map shared memory: " << strerror(errno) << std::endl;
            close(shm_fd_);
            sem_close(semaphore_);
            return false;
        }

        close(shm_fd_); // We don't need the fd anymore
        return true;
    }

    bool lock() {
        if (sem_wait(semaphore_) == -1) {
            std::cerr << "Failed to lock semaphore: " << strerror(errno) << std::endl;
            return false;
        }
        return true;
    }

    bool unlock() {
        if (sem_post(semaphore_) == -1) {
            std::cerr << "Failed to unlock semaphore: " << strerror(errno) << std::endl;
            return false;
        }
        return true;
    }

    std::string read_data() {
        if (!shm_ptr_) return "";
        return std::string(static_cast<char*>(shm_ptr_));
    }

    bool write_data(const std::string& data) {
        if (!shm_ptr_ || data.size() >= shm_size_) {
            return false;
        }
        std::memset(shm_ptr_, 0, shm_size_);
        std::memcpy(shm_ptr_, data.c_str(), data.size());
        return true;
    }

    void cleanup() {
        if (shm_ptr_ && shm_ptr_ != MAP_FAILED) {
            munmap(shm_ptr_, shm_size_);
            shm_ptr_ = nullptr;
        }
        if (semaphore_ && semaphore_ != SEM_FAILED) {
            sem_close(semaphore_);
            semaphore_ = nullptr;
        }
    }
};

// RAII wrapper for automatic lock/unlock
class ScopedLock {
private:
    SharedMemorySync& sync_;
    bool locked_;

public:
    ScopedLock(SharedMemorySync& sync) : sync_(sync), locked_(false) {
        locked_ = sync_.lock();
    }

    ~ScopedLock() {
        if (locked_) {
            sync_.unlock();
        }
    }

    bool is_locked() const { return locked_; }
};

#endif // SHARED_MEMORY_SYNC_HPP
