cmake_minimum_required(VERSION 3.16)  # Changed for better C++20 support

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

# Check for format.cmake existence
set(FORMAT_CMAKE "${CMAKE_SOURCE_DIR}/cmake/format.cmake")
if(NOT EXISTS ${FORMAT_CMAKE})
    message(WARNING "format.cmake not found at ${FORMAT_CMAKE}")
else()
    include(${FORMAT_CMAKE})
endif()

# Explicitly list source files instead of using GLOB
# Use GLOB_RECURSE to find all source files
file(GLOB_RECURSE SOURCES
    "src/*.cpp"
    "sample/*.cpp"
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Enable auto-formatting
enable_auto_format(${PROJECT_NAME})

# Add test directory if it exists
if(EXISTS "${CMAKE_SOURCE_DIR}/tests/CMakeLists.txt")
    add_subdirectory(tests)
endif()

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

install(DIRECTORY include/
    DESTINATION include
)