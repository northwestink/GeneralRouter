#pragma once
#include <cstring>
#include <errno.h>
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

/**
 * @brief A zero-copy circular buffer implementation for efficient network I/O
 *
 * This class implements a circular (ring) buffer that provides zero-copy
 * read/write operations for network socket data. The buffer automatically
 * handles wrapping around when reaching the end of its capacity.
 */
class CircularBuffer {
public:
  /**
   * @brief Constructs a circular buffer with specified capacity
   * @param capacity The size of the buffer in bytes
   * @throw std::bad_alloc if memory allocation fails
   */
  explicit CircularBuffer(size_t capacity)
      : buffer_(new char[capacity]), capacity_(capacity), head_(0), tail_(0),
        full_(false) {
    if (!buffer_) {
      throw std::bad_alloc();
    }
  }

  // Delete copy operations
  CircularBuffer(const CircularBuffer &) = delete;
  CircularBuffer &operator=(const CircularBuffer &) = delete;

  // Add move operations
  CircularBuffer(CircularBuffer &&other) noexcept
      : buffer_(other.buffer_), capacity_(other.capacity_), head_(other.head_),
        tail_(other.tail_), full_(other.full_) {
    other.buffer_ = nullptr;
    other.capacity_ = 0;
  }

  CircularBuffer &operator=(CircularBuffer &&other) noexcept {
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

  ~CircularBuffer() { delete[] buffer_; }

  /**
   * @brief Reads data from socket into buffer using zero-copy operations
   * @param socketFd The socket file descriptor to read from
   * @return Number of bytes read, -1 on error, 0 on connection closed
   * @throws runtime_error if buffer is corrupted
   */
  ssize_t writeFromSocket(int socketFd) {
    if (!buffer_) {
      throw std::runtime_error("Buffer not initialized");
    }

    size_t available = availableSpace();
    if (available == 0) {
      return -1; // Buffer full
    }

    // Calculate optimal write size
    size_t writeSize = std::min(available, capacity_ - tail_);
    ssize_t bytesRead = read(socketFd, buffer_ + tail_, writeSize);

    if (bytesRead > 0) {
      tail_ = (tail_ + bytesRead) % capacity_;
      full_ = (tail_ == head_);

      // Validate buffer state
      if (tail_ >= capacity_ || head_ >= capacity_) {
        throw std::runtime_error("Buffer index corruption detected");
      }
    }

    return bytesRead;
  }

  /**
   * @brief Reads data from buffer into a string
   * @param length Number of bytes to read
   * @return A string containing the read data
   * @throws None
   */
  ssize_t writeFromString(const std::string_view &data) {
    return writeFromBytes(data.data(), data.size());
  }

  /**
   * @brief Writes data from a byte array into the buffer
   * @param data Pointer to the data to write
   * @param length Number of bytes to write
   * @return Number of bytes written, or -1 if buffer is full
   * @throws None
   */
  ssize_t writeFromBytes(const char *data, size_t length) {
    size_t available = availableSpace();
    if (available <= 0) {
      std::cerr << "Buffer full (capacity=" << capacity_ << ", head=" << head_
                << ", tail=" << tail_ << ")" << std::endl;
      return -1;
    }

    // Calculate contiguous writable space
    size_t writeSize = std::min(available, capacity_ - tail_);
    if (writeSize < length) {
      std::cerr << "Data too large for buffer (data=" << length
                << ", available=" << writeSize << ")" << std::endl;
      length = writeSize;
    }

    std::copy(data, data + length, buffer_ + tail_);
    tail_ = (tail_ + length) % capacity_;
    full_ = (tail_ == head_);

    std::cout << "Wrote " << length << " bytes into buffer" << std::endl;
    return length;
  }

  /**
   * @brief Writes a single byte into the buffer
   * @param data The byte to write into the buffer
   * @return Number of bytes written (1 on success, -1 if buffer is full)
   * @throws None
   */
  ssize_t writeFromByte(char data) {
    size_t available = availableSpace();
    if (available <= 0) {
      std::cerr << "Buffer full (capacity=" << capacity_ << ", head=" << head_
                << ", tail=" << tail_ << ")" << std::endl;
      return -1;
    }

    buffer_[tail_] = data;
    tail_ = (tail_ + 1) % capacity_;
    full_ = (tail_ == head_);

    std::cout << "Wrote 1 byte into buffer" << std::endl;
    return 1;
  }

  /**
   * @brief Writes data from buffer to socket using zero-copy operations
   * @param socketFd The socket file descriptor to write to
   * @return Number of bytes written, or -1 on error
   * @throws None
   * @note This method automatically advances the read pointer (head_) on
   * successful write
   * @note If the write is partial, only the successfully written bytes are
   * consumed
   */
  ssize_t readToSocket(int socketFd) {
    size_t available = dataSize();
    if (available <= 0) {
      std::cerr << "Buffer empty, nothing to read" << std::endl;
      return -1;
    }

    // Calculate contiguous readable space
    size_t readSize = std::min(available, capacity_ - head_);
    ssize_t bytesWritten = write(socketFd, buffer_ + head_, readSize);

    if (bytesWritten > 0) {
      std::cout << "Wrote " << bytesWritten << " bytes to socket" << std::endl;
      head_ = (head_ + bytesWritten) % capacity_;
      full_ = false;
    } else if (bytesWritten < 0) {
      std::cerr << "Socket write error: " << strerror(errno) << std::endl;
    }

    return bytesWritten;
  }

  /**
   * @brief Returns current amount of data in the buffer
   * @return Number of bytes available for reading
   */
  size_t dataSize() const {
    if (full_)
      return capacity_;
    return (tail_ >= head_) ? (tail_ - head_) : (capacity_ - head_ + tail_);
  }

  /**
   * @brief Checks if the buffer is empty
   * @return true if buffer is empty, false otherwise
   */
  bool empty() const { return head_ == tail_; };

  /**
   * @brief Gets a read-only view of the buffer data without copying
   * @param start Output parameter for start position
   * @param data Output parameter for data pointer
   * @param length Output parameter for available data length
   * @return true if data is available, false if buffer is empty
   */
  bool getReadView(size_t &start, char *&data, size_t &length) const {
    if (dataSize() == 0) {
      std::cout << "Buffer empty, no data to read" << std::endl;
      return false;
    }

    start = head_;
    data = buffer_ + head_;
    length = (tail_ >= head_) ? (tail_ - head_) : (capacity_ - head_);

    std::cout << "Provided read view: offset=" << start << ", length=" << length
              << std::endl;
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
                << " (requested=" << bytes << ", available=" << available << ")"
                << std::endl;
      bytes = available;
    }

    head_ = (head_ + bytes) % capacity_;
    full_ = false;

    std::cout << "Consumed " << bytes << " bytes, new head=" << head_
              << std::endl;
  }

private:
  char *buffer_;    // Underlying buffer storage
  size_t capacity_; // Total buffer capacity
  size_t head_;     // Read position
  size_t tail_;     // Write position
  bool full_;       // Buffer full flag

  /**
   * @brief Calculates available space for writing
   * @return Number of bytes available for writing
   */
  size_t availableSpace() const {
    if (full_)
      return 0;
    return (tail_ >= head_) ? (capacity_ - (tail_ - head_)) : (head_ - tail_);
  }
};
