#include "tcpclient.h"
#include <errno.h>  // For errno
#include <string.h> // For strerror

// Constructor: Initializes the epoll instance.
TCPClient::TCPClient() { initEpoll(); }

// Destructor: Closes the connection and the epoll file descriptor.
TCPClient::~TCPClient() {
  stopThreads();
  disconnect(); // Close the socket if it's open
  if (epoll_fd_ >= 0) {
    close(epoll_fd_); // Release the epoll file descriptor
  }
}

// Initializes the epoll instance.
void TCPClient::initEpoll() {
  epoll_fd_ = epoll_create1(0); // Create an epoll instance
  if (epoll_fd_ == -1) {
    throw std::runtime_error(
        "Failed to create epoll fd"); // Throw an error if epoll creation fails
  }
}

// Sets the given file descriptor to non-blocking mode.
void TCPClient::setNonBlocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);      // Get the current flags
  fcntl(fd, F_SETFL, flags | O_NONBLOCK); // Add the non-blocking flag
}

// Attempts to establish a TCP connection to the specified host and port.
// Calls the provided handler with the connection status (success/failure).
void TCPClient::connect(const std::string &host, uint16_t port,
                        ConnectHandler handler) {
  connect_handler_ = handler; // Store the connect handler

  sock_fd_ = socket(AF_INET, SOCK_STREAM, 0); // Create a TCP socket
  if (sock_fd_ == -1) {
    connect_handler_(false); // Call the handler with failure status
    return;
  }

  setNonBlocking(sock_fd_); // Set the socket to non-blocking mode

  struct sockaddr_in addr;     // Create an address structure
  addr.sin_family = AF_INET;   // Set the address family to IPv4
  addr.sin_port = htons(port); // Set the port
  if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) {
    close(sock_fd_);         // Close the socket
    connect_handler_(false); // Call the handler with failure status
    return;
  }

  current_event_.events =
      EPOLLIN | EPOLLOUT |
      EPOLLET; // Set the event types to monitor (read, write, edge-triggered)
  current_event_.data.fd = sock_fd_; // Set the file descriptor for the event
  if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, sock_fd_, &current_event_) == -1) {
    close(sock_fd_);         // Close the socket
    connect_handler_(false); // Call the handler with failure status
    return;
  }

  // Initiate the connection
  if (::connect(sock_fd_, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
    // If the connection fails immediately and the error is not EINPROGRESS,
    // it's an error
    if (errno != EINPROGRESS) {
      close(sock_fd_);         // Close the socket
      connect_handler_(false); // Call the handler with failure status
      return;
    }
  }
}

// Closes the TCP connection.
void TCPClient::disconnect() {
  if (sock_fd_ >= 0) {
    // Remove socket from epoll monitoring and clean up resources
    epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, sock_fd_, nullptr);
    close(sock_fd_);
    sock_fd_ = -1;
  }
  connected_ = false;
}

// Process pending socket events using epoll
void TCPClient::poll() {
  // Only handle connection events
  int nfds = epoll_wait(epoll_fd_, events_.data(), events_.size(), 0);

  for (int i = 0; i < nfds; ++i) {
    if (events_[i].events & EPOLLERR || events_[i].events & EPOLLHUP) {
      disconnect();
      if (connect_handler_) {
        connect_handler_(false);
      }
    } else if (!connected_ && (events_[i].events & EPOLLOUT)) {
      connected_ = true;
      if (connect_handler_) {
        connect_handler_(true);
        startThreads(); // Start threads after successful connection
      }
    }
  }
}

// Reads data from the socket.
void TCPClient::doRead() {
  while (true) {
    ssize_t bytes = read(sock_fd_, read_buffer_.data(),
                         max_buffer_size); // Read data from the socket
    if (bytes > 0 && message_handler_) {
      message_handler_(std::string(
          read_buffer_.data(),
          bytes)); // Call the message handler with the received data
    } else if (bytes == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        break; // No more data to read
      }
      disconnect(); // Disconnect on error
      break;
    } else if (bytes == 0) {
      disconnect(); // Disconnect if the connection was closed by the peer
      break;
    }
  }
}

// Asynchronously sends a message to the connected server.
// The message is added to a queue and sent when the socket is available for
// writing.
void TCPClient::asyncSend(const std::string &message) {
  std::lock_guard<std::mutex> lock(
      write_mutex_);                 // Protect the write queue with a mutex
  write_queue_.push_back(message);   // Add the message to the write queue
  current_event_.events |= EPOLLOUT; // Add EPOLLOUT to the event mask
  epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, sock_fd_,
            &current_event_); // Modify the epoll event for the socket
}

// Writes data to the socket.
void TCPClient::doWrite() {
  std::lock_guard<std::mutex> lock(
      write_mutex_);              // Protect the write queue with a mutex
  while (!write_queue_.empty()) { // While there are messages in the queue
    const std::string &message =
        write_queue_.front(); // Get the first message from the queue
    ssize_t bytes = write(sock_fd_, message.data(),
                          message.size()); // Write the message to the socket
    if (bytes > 0) {
      write_queue_.pop_front(); // Remove the message from the queue
    } else if (bytes == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        break; // No more data can be written
      }
      disconnect(); // Disconnect on error
      break;
    }
  }

  // If the write queue is empty, remove EPOLLOUT from the event mask
  if (write_queue_.empty()) {
    current_event_.events &= ~EPOLLOUT; // Remove EPOLLOUT from the event mask
    epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, sock_fd_,
              &current_event_); // Modify the epoll event for the socket
  }
}

// Sets the message handler function.
void TCPClient::setMessageHandler(MessageHandler handler) {
  message_handler_ = handler; // Store the message handler
}

// Returns true if the client is currently connected to the server, false
// otherwise.
bool TCPClient::isConnected() const { return connected_; }

void TCPClient::startThreads() {
  std::lock_guard<std::mutex> lock(stop_mutex_);
  should_stop_ = false;
  receive_thread_ = std::thread(&TCPClient::receiveLoop, this);
  send_thread_ = std::thread(&TCPClient::sendLoop, this);
}

void TCPClient::stopThreads() {
  {
    std::lock_guard<std::mutex> lock(stop_mutex_);
    should_stop_ = true;
  }

  if (receive_thread_.joinable()) {
    receive_thread_.join();
  }
  if (send_thread_.joinable()) {
    send_thread_.join();
  }
}

void TCPClient::receiveLoop() {
  while (true) {
    {
      std::lock_guard<std::mutex> lock(stop_mutex_);
      if (should_stop_)
        break;
    }

    // Check for read events
    struct epoll_event events[1];
    int nfds = epoll_wait(epoll_fd_, events, 1, 100); // 100ms timeout

    if (nfds > 0 && (events[0].events & EPOLLIN)) {
      doRead();
    }
  }
}

void TCPClient::sendLoop() {
  while (true) {
    {
      std::lock_guard<std::mutex> lock(stop_mutex_);
      if (should_stop_)
        break;
    }

    // Check for write events if there's data to send
    {
      std::lock_guard<std::mutex> lock(write_mutex_);
      if (!write_queue_.empty()) {
        doWrite();
      }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}
