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

// Local includes
#include "worker.h"

/**
 * @class TcpServer
 * @brief A multi-threaded TCP server using epoll for event handling
 *
 * This server uses a thread pool of workers to handle incoming connections
 * and implements a round-robin distribution of connections across workers.
 */
class TcpServer {
private:
  int listenFd;
  int epollFd;
  std::atomic<bool> shutdownFlag;
  std::vector<WorkerThread> workers;
  std::vector<std::jthread> workerThreads;
  std::vector<int> workerPipesWrite;
  std::atomic<int> roundRobinIndex;

  /**
   * @brief Initializes the listening socket
   * @param __hostshort Port number to listen on
   * @throws Exits program on socket creation/binding failure
   */
  void initListenSocket(uint16_t __hostshort) {
    // Create TCP/IP socket
    listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd == -1) {
      std::cerr << "Failed to create socket: " << strerror(errno) << std::endl;
      exit(1);
    }

    // Set socket to non-blocking mode for async operation
    if (fcntl(listenFd, F_SETFL, O_NONBLOCK) == -1) {
      std::cerr << "Failed to set non-blocking mode: " << strerror(errno)
                << std::endl;
      close(listenFd);
      exit(1);
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(__hostshort);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(listenFd, (struct sockaddr *)&addr, sizeof addr) == -1) {
      std::cerr << "Failed to bind socket: " << strerror(errno) << std::endl;
      close(listenFd);
      exit(1);
    }

    if (listen(listenFd, SOMAXCONN) == -1) {
      std::cerr << "Failed to listen on socket: " << strerror(errno)
                << std::endl;
      close(listenFd);
      exit(1);
    }
  }

  void initEpoll() {
    epollFd = epoll_create1(0);
    if (epollFd == -1) {
      std::cerr << "epoll_create1 failed" << std::endl;
      exit(1);
    }

    epoll_event event;
    event.data.fd = listenFd;
    event.events = EPOLLIN | EPOLLET;
    if (epoll_ctl(epollFd, EPOLL_CTL_ADD, listenFd, &event) == -1) {
      std::cerr << "epoll_ctl failed for listenFd" << std::endl;
      close(epollFd);
      exit(1);
    }
  }

public:
  TcpServer(uint16_t __hostshort) : shutdownFlag(false), roundRobinIndex(0) {
    initListenSocket(__hostshort);
    initEpoll();

    int numWorkers = std::thread::hardware_concurrency();
    if (numWorkers == 0)
      numWorkers = 1;

    // workers.resize(numWorkers);
    for (int i = 0; i < numWorkers; ++i) {
      workers.emplace_back(shutdownFlag);
      workerPipesWrite.push_back(workers.back().getPipeWriteFd());
      workerThreads.emplace_back([this, i]() { workers[i].run(); });
    }
  }

  void run() {
    while (!shutdownFlag) {
      epoll_event events[1];
      int numEvents = epoll_wait(epollFd, events, 1, 1000);
      if (numEvents == -1) {
        std::cerr << "epoll_wait failed" << std::endl;
        break;
      }
      if (numEvents == 0)
        continue;

      if (events[0].data.fd == listenFd && events[0].events & EPOLLIN) {
        int newFd = accept(listenFd, nullptr, nullptr);
        if (newFd == -1) {
          std::cerr << "accept failed" << std::endl;
          continue;
        }

        fcntl(newFd, F_SETFL, O_NONBLOCK);
        int workerIndex = roundRobinIndex++ % workers.size();
        ssize_t bytesWritten =
            write(workerPipesWrite[workerIndex], &newFd, sizeof(newFd));
        if (bytesWritten != sizeof(newFd)) {
          std::cerr << "write to pipe failed" << std::endl;
          close(newFd);
        }
      }
    }
  }

  ~TcpServer() {
    shutdownFlag = true;
    for (auto &thread : workerThreads) {
      thread.join();
    }
    close(epollFd);
    close(listenFd);
  }
};
