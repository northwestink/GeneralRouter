#pragma once

// System includes
#include <sys/epoll.h>    // For epoll functionality
#include <sys/socket.h>   // For socket creation and manipulation
#include <netinet/in.h>   // For internet address family
#include <arpa/inet.h>    // For converting IP addresses
#include <unistd.h>       // For close() function
#include <fcntl.h>        // For file control options like non-blocking

// Standard library includes
#include <memory>         // For smart pointers
#include <string>         // For string manipulation
#include <functional>     // For std::function
#include <deque>          // For double-ended queue
#include <mutex>          // For thread safety
#include <array>          // For fixed-size array
#include <thread>         // For thread support

/**
 * @brief TCP client implementation with non-blocking I/O and event-driven architecture
 */
class TCPClient {
public:
    /** Type alias for message handler callback function */
    using MessageHandler = std::function<void(const std::string&)>;
    
    /** Type alias for connection status callback function */
    using ConnectHandler = std::function<void(bool)>;

    /** @brief Constructs a new TCP client instance */
    TCPClient();

    /**
     * @brief Destroys the TCP client instance and releases resources
     * 
     * Performs cleanup of system resources:
     * - Closes active TCP connection
     * - Closes epoll file descriptor
     * - Clears pending messages
     */
    ~TCPClient();

    /**
     * @brief Establishes TCP connection to specified endpoint
     * @param host Server hostname or IP address
     * @param port Server port number
     * @param handler Callback for connection status
     */
    void connect(const std::string& host, uint16_t port, ConnectHandler handler);

    /** @brief Terminates the current TCP connection */
    void disconnect();

    /**
     * @brief Queues a message for asynchronous transmission
     * @param message Data to be sent
     */
    void asyncSend(const std::string& message);

    /**
     * @brief Sets callback for handling incoming messages
     * @param handler Function to process received messages
     */
    void setMessageHandler(MessageHandler handler);

    /** @brief Returns current connection status */
    bool isConnected() const;

    /** @brief Processes pending socket events */
    void poll();

    /** @brief Starts the message processing threads */
    void startThreads();
    
    /** @brief Stops the message processing threads */
    void stopThreads();

private:
    /** @brief Sets up epoll instance for event monitoring */
    void initEpoll();
    
    /**
     * @brief Configures socket for non-blocking operation
     * @param fd File descriptor to modify
     */
    void setNonBlocking(int fd);
    
    /** @brief Processes events from epoll */
    void handleEvents();
    
    /** @brief Handles incoming data from socket */
    void doRead();
    
    /** @brief Processes outgoing data queue */
    void doWrite();

    /** @brief Thread function for receiving messages */
    void receiveLoop();
    
    /** @brief Thread function for sending messages */
    void sendLoop();

    // File descriptors and state flags
    int epoll_fd_{-1};           ///< Epoll instance descriptor
    int sock_fd_{-1};            ///< Socket descriptor
    bool connected_{false};       ///< Connection state
    bool writing_{false};         ///< Write operation state

    // Message queue management
    std::deque<std::string> write_queue_;    ///< Pending messages
    std::mutex write_mutex_;                  ///< Queue synchronization

    // Buffer management
    static constexpr size_t max_buffer_size = 8192;
    std::array<char, max_buffer_size> read_buffer_;  ///< Receive buffer

    // Event handling
    MessageHandler message_handler_;          ///< Message processing callback
    ConnectHandler connect_handler_;          ///< Connection status callback
    struct epoll_event current_event_;        ///< Current event being processed
    std::array<struct epoll_event, 1> events_; ///< Event buffer

    // Thread management
    std::thread receive_thread_;    ///< Thread for receiving messages
    std::thread send_thread_;       ///< Thread for sending messages
    bool should_stop_{false};       ///< Thread control flag
    std::mutex stop_mutex_;         ///< Mutex for thread control
};
