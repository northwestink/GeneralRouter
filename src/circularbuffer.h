#pragma once
#include <iostream>
#include <vector>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>

/**
 * @brief A zero-copy circular buffer implementation for efficient network I/O
 * 
 * This class implements a circular (ring) buffer that provides zero-copy read/write
 * operations for network socket data. The buffer automatically handles wrapping
 * around when reaching the end of its capacity.
 */
class CircularBuffer {
public:
    /**
     * @brief Constructs a circular buffer with specified capacity
     * @param capacity The size of the buffer in bytes
     * @throw std::bad_alloc if memory allocation fails
     */
    explicit CircularBuffer(size_t capacity)
        : buffer_(new char[capacity]), capacity_(capacity), head_(0), tail_(0), full_(false) {
        if (!buffer_) {
            throw std::bad_alloc();
        }
    }

    // Delete copy operations
    CircularBuffer(const CircularBuffer&) = delete;
    CircularBuffer& operator=(const CircularBuffer&) = delete;

    // Add move operations
    CircularBuffer(CircularBuffer&& other) noexcept
        : buffer_(other.buffer_)
        , capacity_(other.capacity_)
        , head_(other.head_)
        , tail_(other.tail_)
        , full_(other.full_) {
        other.buffer_ = nullptr;
        other.capacity_ = 0;
    }

    CircularBuffer& operator=(CircularBuffer&& other) noexcept {
        if (this != &other) {
            delete[] buffer_;
            buffer_ = other.buffer_;
            capacity_ = other.capacity_;
            head_ = other.head_;
            tail_ = other.tail_;
            full_ = other.full_;
            other.buffer_ = nullptr;
            other.capacity_ = 0;
        }
        return *this;
    }

    ~CircularBuffer() {
        delete[] buffer_;
    }

    /**
     * @brief Reads data from socket into buffer using zero-copy operations
     * @param socketFd The socket file descriptor to read from
     * @return Number of bytes read, or -1 on error
     */
    ssize_t writeFromSocket(int socketFd) {
        size_t available = availableSpace();
        if (available <= 0) {
            std::cerr << "Buffer full (capacity=" << capacity_ 
                      << ", head=" << head_ 
                      << ", tail=" << tail_ << ")" << std::endl;
            return -1;
        }

        // Calculate contiguous writable space
        size_t writeSize = std::min(available, capacity_ - tail_);
        ssize_t bytesRead = read(socketFd, buffer_ + tail_, writeSize);
        
        if (bytesRead > 0) {
            std::cout << "Read " << bytesRead << " bytes into buffer" << std::endl;
            tail_ = (tail_ + bytesRead) % capacity_;
            full_ = (tail_ == head_);
        } else if (bytesRead < 0) {
            std::cerr << "Socket read error: " << strerror(errno) << std::endl;
        }
        
        return bytesRead;
    }

    /**
     * @brief Returns current amount of data in the buffer
     * @return Number of bytes available for reading
     */
    size_t dataSize() const {
        if (full_) return capacity_;
        return (tail_ >= head_) ? (tail_ - head_) : (capacity_ - head_ + tail_);
    }

    /**
     * @brief Gets a read-only view of the buffer data without copying
     * @param start Output parameter for start position
     * @param data Output parameter for data pointer
     * @param length Output parameter for available data length
     * @return true if data is available, false if buffer is empty
     */
    bool getReadView(size_t& start, char*& data, size_t& length) const {
        if (dataSize() == 0) {
            std::cout << "Buffer empty, no data to read" << std::endl;
            return false;
        }
        
        start = head_;
        data = buffer_ + head_;
        length = (tail_ >= head_) ? (tail_ - head_) : (capacity_ - head_);
        
        std::cout << "Provided read view: offset=" << start 
                  << ", length=" << length << std::endl;
        return true;
    }

    /**
     * @brief Marks data as consumed, moving the read pointer forward
     * @param bytes Number of bytes to consume
     */
    void consume(size_t bytes) {
        size_t available = dataSize();
        if (bytes > available) {
            std::cerr << "Warning: Attempting to consume more bytes than available" 
                      << " (requested=" << bytes 
                      << ", available=" << available << ")" << std::endl;
            bytes = available;
        }
        
        head_ = (head_ + bytes) % capacity_;
        full_ = false;
        
        std::cout << "Consumed " << bytes << " bytes, new head=" << head_ << std::endl;
    }

private:
    char* buffer_;      // Underlying buffer storage
    size_t capacity_;   // Total buffer capacity
    size_t head_;      // Read position
    size_t tail_;      // Write position
    bool full_;        // Buffer full flag

    /**
     * @brief Calculates available space for writing
     * @return Number of bytes available for writing
     */
    size_t availableSpace() const {
        if (full_) return 0;
        return (tail_ >= head_) ? 
               (capacity_ - (tail_ - head_)) : 
               (head_ - tail_);
    }
};

