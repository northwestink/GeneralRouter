# Enable testing support
enable_testing()

# Set project
project(GeneralRouterTests)

# Compiler flags
if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -DNDEBUG")
elseif(MSVC)
  # /W4 enables most warnings
  # /WX treats warnings as errors
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /WX")
endif()

# Google Test
# Add Google Test
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG release-1.12.1
)
FetchContent_MakeAvailable(googletest)

# Circular Buffer Tests
add_library(circularbuffer INTERFACE)
target_include_directories(circularbuffer INTERFACE ${CMAKE_SOURCE_DIR}/src)

# Add test executable
add_executable(circularbuffer_test circularbuffer_test.cpp)

# Modern CMake: Use target_sources to specify source files
target_sources(circularbuffer_test
    PRIVATE
        circularbuffer_test.cpp
)

target_link_libraries(circularbuffer_test
    PRIVATE
    circularbuffer
    gtest
    gtest_main
)

# Set C++ standard for test executable
set_target_properties(circularbuffer_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Register test
add_test(NAME circularbuffer_test COMMAND $<TARGET_FILE:circularbuffer_test>)