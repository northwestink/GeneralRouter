#pragma once

// Standard library includes
#include <atomic>
#include <iostream>
#include <map>
#include <thread>
#include <vector>

// System includes
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "connection.h"

constexpr int MAX_EVENTS = 1024;
constexpr size_t BUFSIZE = 1024 * 1024;

class WorkerThread {
private:
  int epollFd;                           // Epoll file descriptor
  int pipeReadFd;                        // Read end of control pipe
  int pipeWriteFd;                       // Write end of control pipe
  std::map<int, Connection> connections; // Active connections
  std::atomic<bool> &shutdownFlag;       // Shutdown signal

  // Prevent copying
  WorkerThread(const WorkerThread &) = delete;
  WorkerThread &operator=(const WorkerThread &) = delete;

  /**
   * Closes and cleans up a connection
   * @param fd Socket file descriptor to close
   */
  void closeConnection(int fd) {
    epoll_ctl(epollFd, EPOLL_CTL_DEL, fd, nullptr);
    close(fd);
    connections.erase(fd);
  }

  /**
   * Updates epoll events for a file descriptor
   * @param fd File descriptor to modify
   * @param events New event mask
   */
  void setEpollEvents(int fd, int events) {
    epoll_event event;
    event.data.fd = fd;
    event.events = events | EPOLLET;
    if (epoll_ctl(epollFd, EPOLL_CTL_MOD, fd, &event) == -1) {
      std::cerr << "epoll_ctl failed to modify event" << std::endl;
    }
  }

  /**
   * Processes a logon message and prepares response
   * @param message Incoming FIX message
   * @param conn Connection to write response to
   */
  void processLogon(Connection &conn) {
    Message &message = conn.message;

    // Prepare response
    conn.writeBuffer.writeFromString(Message::BeginString);
    conn.writeBuffer.writeFromString(message.beginString);
    conn.writeBuffer.writeFromByte(Message::SOH);

    // BodyLength
    conn.writeBuffer.writeFromString(Message::BodyLength);
    conn.writeBuffer.writeFromString(message.bodyLength);
    conn.writeBuffer.writeFromByte(Message::SOH);

    // send Msgtype
    conn.writeBuffer.writeFromString(Message::MsgType);
    conn.writeBuffer.writeFromString(message.msgType);
    conn.writeBuffer.writeFromByte(Message::SOH);

    // send SeqNumber
    conn.writeBuffer.writeFromString(Message::SeqNumber);
    conn.writeBuffer.writeFromString(message.seqNumber);
    conn.writeBuffer.writeFromByte(Message::SOH);

    // send SenderCompID
    conn.writeBuffer.writeFromString(Message::SenderCompID);
    conn.writeBuffer.writeFromString(message.targetCompID);
    conn.writeBuffer.writeFromByte(Message::SOH);

    // send TargetCompID
    conn.writeBuffer.writeFromString(Message::TargetCompID);
    conn.writeBuffer.writeFromString(message.senderCompID);
    conn.writeBuffer.writeFromByte(Message::SOH);

    // send OtherFields
    for (const auto &field : message.otherFields) {
      conn.writeBuffer.writeFromString(field.first);
      conn.writeBuffer.writeFromByte('=');
      conn.writeBuffer.writeFromString(field.second);
      conn.writeBuffer.writeFromByte(Message::SOH);
    }

    // checkSum
    conn.writeBuffer.writeFromString(Message::CheckSum);
    conn.writeBuffer.writeFromString(message.checkSum);
    conn.writeBuffer.writeFromByte(Message::SOH);
  }

  /**
   * Processes different message types
   * @param message Parsed FIX message
   * @param conn Connection context
   */
  void processData(Connection &conn) {
    // Beging String
    if ("A" == conn.message.msgType) {
      processLogon(conn);
    }
  }

  /**
   * Processes incoming FIX stream with zero-copy parsing
   * @param socketFd Socket file descriptor to read from
   * @return true if a complete message was processed, false otherwise
   * @throws runtime_error on critical errors
   */
  bool processFixStream(int socketFd) {
    auto connectionIt = connections.find(socketFd);
    if (connectionIt == connections.end()) {
      auto [it, inserted] = connections.try_emplace(socketFd, BUFSIZE);
      if (!inserted) {
        throw std::runtime_error("Failed to create connection");
      }
      connectionIt = it;
    }

    CircularBuffer &readBuffer = connectionIt->second.readBuffer;
    Message &message = connectionIt->second.message;

    while (true) {
      // Read socket data with improved error handling
      ssize_t bytesRead = readBuffer.writeFromSocket(socketFd);
      if (bytesRead < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          return false; // No more data available
        }
        std::cerr << "Fatal socket error: " << strerror(errno) << std::endl;
        closeConnection(socketFd);
        return false;
      }
      if (bytesRead == 0) {
        std::cout << "Connection closed by peer." << std::endl;
        closeConnection(socketFd);
        return false;
      }

      // Parse FIX messages
      while (true) {
        std::cout << "Parsing FIX message..." << std::endl;
        switch (Message::parseFixMessage(readBuffer, message)) {
        case ParseResult::FINISHED: {
          // Debug logging for parsed message
          std::cout << "Parsed FIX message:" << std::endl
                    << "BeginString:" << message.beginString << std::endl
                    << "BodyLength:" << message.bodyLength << std::endl
                    << "CheckSum:" << message.checkSum << std::endl
                    << "MsgType:" << message.msgType << std::endl
                    << "SenderCompID:" << message.senderCompID << std::endl
                    << "TargetCompID:" << message.targetCompID << std::endl
                    << "ClordID:" << message.clOrdID << std::endl
                    << "SeqNumber:" << message.seqNumber << std::endl
                    << "Other fields:" << message.otherFields.size()
                    << std::endl;
          for (const auto &field : message.otherFields) {
            std::cout << field.first << ":" << field.second << std::endl;
          }

          // Process complete message
          processData(connectionIt->second);
          message.reset();
          return true;
        }
        default:
          return false;
        }
      }
    }
  }

public:
  /**
   * Constructor initializes epoll and control pipe
   * @param sf Reference to shutdown flag
   */
  WorkerThread(std::atomic<bool> &sf) : shutdownFlag(sf) {
    epollFd = epoll_create1(0);
    if (epollFd == -1) {
      std::cerr << "epoll_create1 failed" << std::endl;
      exit(1);
    }

    int pipeFds[2];
    if (pipe(pipeFds) == -1) {
      std::cerr << "pipe failed" << std::endl;
      exit(1);
    }
    pipeReadFd = pipeFds[0];
    pipeWriteFd = pipeFds[1];

    fcntl(pipeReadFd, F_SETFL, O_NONBLOCK);

    epoll_event event;
    event.data.fd = pipeReadFd;
    event.events = EPOLLIN | EPOLLET;
    if (epoll_ctl(epollFd, EPOLL_CTL_ADD, pipeReadFd, &event) == -1) {
      std::cerr << "epoll_ctl failed for pipe" << std::endl;
      exit(1);
    }
  }

  // Support move operations
  WorkerThread(WorkerThread &&) = default;            // 移动构造函数（可选）
  WorkerThread &operator=(WorkerThread &&) = default; // 移动赋值运算符

  int getPipeWriteFd() const { return pipeWriteFd; }

  /**
   * Main worker loop
   * Handles epoll events and processes messages
   */
  void run() {
    while (!shutdownFlag) {
      epoll_event events[MAX_EVENTS];
      int numEvents = epoll_wait(epollFd, events, MAX_EVENTS, -1);
      if (numEvents == -1) {
        std::cerr << "epoll_wait failed" << std::endl;
        break;
      }

      for (int i = 0; i < numEvents; ++i) {
        if (events[i].data.fd == pipeReadFd) {
          int newFd;
          ssize_t bytesRead = read(pipeReadFd, &newFd, sizeof(newFd));
          if (bytesRead == sizeof(newFd)) {
            auto connectionIt = connections.find(newFd);
            if (connectionIt == connections.end()) {
              auto [it, inserted] = connections.try_emplace(newFd, BUFSIZE);
              connectionIt = it;
            }
            epoll_event event;
            event.data.fd = newFd;
            event.events = EPOLLIN | EPOLLET;
            if (epoll_ctl(epollFd, EPOLL_CTL_ADD, newFd, &event) == -1) {
              std::cerr << "epoll_ctl failed for new connection" << std::endl;
              close(newFd);
              continue;
            }
          } else if (bytesRead == -1 && errno != EAGAIN) {
            std::cerr << "read from pipe failed" << std::endl;
          }
        } else {
          int fd = events[i].data.fd;
          if (events[i].events & EPOLLIN) {
            Connection &conn = connections.at(fd);
            while (processFixStream(fd)) {
            }
            if (!conn.writeBuffer.empty()) {
              setEpollEvents(fd, EPOLLIN | EPOLLOUT);
            } else {
              setEpollEvents(fd, EPOLLIN);
            }
          }
          if (events[i].events & EPOLLOUT) {
            Connection &conn = connections.at(fd);
            while (!conn.writeBuffer.empty()) {
              ssize_t bytesWritten = conn.writeBuffer.readToSocket(fd);
              if (bytesWritten > 0) {
                break;
              } else if (bytesWritten == -1 &&
                         (errno == EAGAIN || EWOULDBLOCK)) {
                break;
              } else {
                std::cerr << "write failed" << std::endl;
                closeConnection(fd);
                break;
              }
            }
            if (conn.writeBuffer.empty()) {
              setEpollEvents(fd, EPOLLIN);
            }
          }
        }
      }
    }
    // 关闭所有连接
    for (const auto &pair : connections) {
      closeConnection(pair.first);
    }
  }
};