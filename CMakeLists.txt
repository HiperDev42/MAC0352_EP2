cmake_minimum_required(VERSION 3.16)
project(MAC0352_EP2 CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror -O2")

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

# Agent executable
add_executable(agent
    src/agents.cpp
    src/server.cpp
    src/metrics.cpp
)
target_link_libraries(agent PRIVATE pthread)

# Manager executable
add_executable(manager
    src/manager.cpp
)

# Clean target for consistency with Makefile
add_custom_target(clean-all
    COMMAND ${CMAKE_MAKE_PROGRAM} clean
    COMMAND rm -rf CMakeFiles CMakeCache.txt cmake_install.cmake
    COMMENT "Cleaning build artifacts..."
)
