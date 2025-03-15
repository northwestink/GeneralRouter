# C++ TCP Server Project

This project implements a TCP server in C++. It utilizes epoll for efficient connection handling and creates worker threads to manage incoming connections.

## Project Structure

```
cpp-project
├── src
│   ├── main.cpp       # Entry point of the application
│   └── tcp.h         # Defines the WorkerThread class for connection management
├── CMakeLists.txt     # CMake configuration file
└── README.md          # Project documentation
```

## Setup Instructions

1. **Clone the repository:**
   ```
   git clone <repository-url>
   cd cpp-project
   ```

2. **Build the project:**
   Ensure you have CMake installed. Run the following commands:
   ```
   mkdir build
   cd build
   cmake ..
   make
   ```

3. **Run the server:**
   After building, you can run the server with:
   ```
   ./cpp-project
   ```

## Usage

The server listens for incoming TCP connections on port 8080. You can connect to it using any TCP client. The server will handle multiple connections concurrently using worker threads.

## Contributing

Feel free to submit issues or pull requests for improvements or bug fixes.

8=FIX.4.29=7035=A34=149=CLIENT152=20250314-15:24:42.19156=EXECUTOR98=0108=3010=088
8=FIX.4.29=7035=A34=249=CLIENT152=20250314-15:24:52.19556=EXECUTOR98=0108=3010=094
8=FIX.4.29=7035=A34=349=CLIENT152=20250314-15:24:54.17656=EXECUTOR98=0108=3010=096
8=FIX.4.29=7035=A34=449=CLIENT152=20250314-15:24:56.15756=EXECUTOR98=0108=3010=098
8=FIX.4.29=7035=A34=549=CLIENT152=20250314-15:24:58.13456=EXECUTOR98=0108=3010=096
8=FIX.4.29=7035=A34=649=CLIENT152=20250314-15:25:00.11056=EXECUTOR98=0108=3010=079


8=FIX.4.29=7010=08835=A49=CLIENT156=EXECUTOR34=1

52=20250314-15:24:42.19198=0108=30