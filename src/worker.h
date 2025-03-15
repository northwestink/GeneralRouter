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

#include "message.h"

constexpr int MAX_EVENTS = 1024;
constexpr size_t BUFSIZE = 1024* 1024;

// Represents a single client connection with read/write buffers
class Connection {
public:
    std::vector<char> readBuffer;   // Buffer for incoming data
    std::vector<char> writeBuffer;  // Buffer for outgoing data
};

class WorkerThread {
private:
    int epollFd;                    // Epoll file descriptor
    int pipeReadFd;                 // Read end of control pipe
    int pipeWriteFd;                // Write end of control pipe
    std::map<int, Connection> connections;  // Active connections
    std::atomic<bool>& shutdownFlag;        // Shutdown signal

    // Prevent copying
    WorkerThread(const WorkerThread&) = delete;
    WorkerThread& operator=(const WorkerThread&) = delete;

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
    void processLogon(const Message& message, Connection& conn) {
        conn.writeBuffer.insert(conn.writeBuffer.end(), Message::BeginString.begin(), Message::BeginString.end());
        conn.writeBuffer.insert(conn.writeBuffer.end(), message.beginString.begin(), message.beginString.end());
        conn.writeBuffer.push_back(Message::SOH);

        conn.writeBuffer.insert(conn.writeBuffer.end(), Message::BodyLength.begin(), Message::BodyLength.end());
        conn.writeBuffer.insert(conn.writeBuffer.end(), message.bodyLength.begin(), message.bodyLength.end());
        conn.writeBuffer.push_back(Message::SOH);

        // send Msgtype
        conn.writeBuffer.insert(conn.writeBuffer.end(), Message::MsgType.begin(), Message::MsgType.end());
        conn.writeBuffer.insert(conn.writeBuffer.end(), message.msgType.begin(), message.msgType.end());
        conn.writeBuffer.push_back(Message::SOH);

        // send SeqNumber
        conn.writeBuffer.insert(conn.writeBuffer.end(), Message::SeqNumber.begin(), Message::SeqNumber.end());
        conn.writeBuffer.insert(conn.writeBuffer.end(), message.seqNumber.begin(), message.seqNumber.end());
        conn.writeBuffer.push_back(Message::SOH);

        // send SenderCompID
        conn.writeBuffer.insert(conn.writeBuffer.end(), Message::SenderCompID.begin(), Message::SenderCompID.end());
        conn.writeBuffer.insert(conn.writeBuffer.end(), message.senderCompID.begin(), message.senderCompID.end());
        conn.writeBuffer.push_back(Message::SOH);

        // send TargetCompID
        conn.writeBuffer.insert(conn.writeBuffer.end(), Message::TargetCompID.begin(), Message::TargetCompID.end());
        conn.writeBuffer.insert(conn.writeBuffer.end(), message.targetCompID.begin(), message.targetCompID.end());
        conn.writeBuffer.push_back(Message::SOH);

        // send OtherFields
        for (const auto& field : message.otherFields) {
            conn.writeBuffer.insert(conn.writeBuffer.end(), field.first.begin(), field.first.end());
            conn.writeBuffer.push_back('=');
            conn.writeBuffer.insert(conn.writeBuffer.end(), field.second.begin(), field.second.end());
            conn.writeBuffer.push_back(Message::SOH);
        }

        // checksum
        conn.writeBuffer.insert(conn.writeBuffer.end(), Message::CheckSum.begin(), Message::CheckSum.end());
        conn.writeBuffer.insert(conn.writeBuffer.end(), message.checksum.begin(), message.checksum.end());
        conn.writeBuffer.push_back(Message::SOH);   
    }

    /**
     * Processes different message types
     * @param message Parsed FIX message
     * @param conn Connection context
     */
    void processData(const Message& message, Connection& conn) {
        // Beging String
        if("A" == message.msgType){
            processLogon(message, conn);
        }
    }

    /**
     * Processes incoming FIX stream with zero-copy parsing
     * @param socketFd Socket file descriptor to read from
     * @return true if a complete message was processed
     */
    bool processFixStream(int socketFd) {
        // Add thread-local buffers for efficient processing
        static thread_local std::map<int, CircularBuffer> buffers;
        static thread_local std::map<int, Message> messages;
        
        // Initialize buffers if needed
        auto bufferIt = buffers.find(socketFd);
        if (bufferIt == buffers.end()) {
            auto [it, inserted] = buffers.try_emplace(socketFd, BUFSIZE);
            bufferIt = it;
        }
        
        if (messages.find(socketFd) == messages.end()) {
            messages.emplace(socketFd, Message());
        }
        
        CircularBuffer& buffer = bufferIt->second;
        Message& message = messages[socketFd];

        while (true) {
            // Read socket data
            ssize_t bytesRead = buffer.writeFromSocket(socketFd);
            std::cout << "Read " << bytesRead << " bytes from socket." << std::endl;
            if (bytesRead < 0) {
                std::cerr << "Error reading from socket: " << strerror(errno) << std::endl;
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    return false;
                }
                std::cerr << "read failed" << std::endl;
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
                switch (Message::parseFixMessage(buffer, message)) {
                    case ParseResult::FINISHED: {
                        // Debug logging for parsed message
                        std::cout << "Parsed FIX message:" << std::endl 
                        << "BeginString:" << message.beginString << std::endl
                        << "BodyLength:" << message.bodyLength << std::endl
                        << "CheckSum:" << message.checksum << std::endl
                        << "MsgType:" << message.msgType << std::endl
                        << "SenderCompID:" << message.senderCompID << std::endl
                        << "TargetCompID:" << message.targetCompID << std::endl
                        << "ClordID:" << message.clOrdID << std::endl
                        << "SeqNumber:" << message.seqNumber << std::endl
                        << "Other fields:" << message.otherFields.size() << std::endl;
                        for (const auto& field : message.otherFields) {
                            std::cout << field.first << ":" << field.second << std::endl;
                        }

                        // Process complete message
                        processData(message, connections[socketFd]);
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
    WorkerThread(std::atomic<bool>& sf) : shutdownFlag(sf) {
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
    WorkerThread(WorkerThread&&) = default; // 移动构造函数（可选）
    WorkerThread& operator=(WorkerThread&&) = default; // 移动赋值运算符

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
                        Connection conn;
                        connections[newFd] = conn;
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
                        Connection& conn = connections[fd];
                        while (processFixStream(fd)) {}
                        if (!conn.writeBuffer.empty()) {
                            setEpollEvents(fd, EPOLLIN | EPOLLOUT);
                        } else {
                            setEpollEvents(fd, EPOLLIN);
                        }
                    }
                    if (events[i].events & EPOLLOUT) {
                        Connection& conn = connections[fd];
                        while (!conn.writeBuffer.empty()) {
                            ssize_t bytesWritten = write(fd, conn.writeBuffer.data(), conn.writeBuffer.size());
                            if (bytesWritten > 0) {
                                conn.writeBuffer.erase(conn.writeBuffer.begin(), conn.writeBuffer.begin() + bytesWritten);
                            } else if (bytesWritten == -1 && (errno == EAGAIN || EWOULDBLOCK)) {
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
        for (const auto& pair : connections) {
            closeConnection(pair.first);
        }
    }
};