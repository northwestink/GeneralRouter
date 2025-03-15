cmake_minimum_required(VERSION 3.10)

# Set project name and version
project(GeneralRouter 
    VERSION 1.0.0
    DESCRIPTION "A General Purpose Router"
    LANGUAGES CXX)

# Require C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set build type if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Add compiler warnings
if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# Include directories
include_directories(
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/include
)

# Explicitly list source files instead of using GLOB
set(SOURCES
    src/main.cpp
    # Add other source files here
)

set(HEADERS
    # Add header files here
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Set output directories
set_target_properties(${PROJECT_NAME}
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

# Enable testing
enable_testing()

# Install rules
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)