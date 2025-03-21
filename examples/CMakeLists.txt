cmake_minimum_required(VERSION 3.10)
project(tcp_client_demo)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add compiler warnings
if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

file(GLOB_RECURSE SOURCES
    ../src/tcpclient.cpp
    tcp_client_demo.cpp 
)


# TCP client demo
add_executable(tcp_client_demo ${SOURCES})
target_link_libraries(tcp_client_demo PRIVATE pthread)

# Set output directory for examples
set_target_properties(tcp_client_demo
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/examples"
)
