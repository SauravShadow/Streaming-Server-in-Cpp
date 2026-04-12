cmake_minimum_required(VERSION 3.10)
project(streaming_server)

set(CMAKE_CXX_STANDARD 17)

# Add submodules
add_subdirectory(libs/network)
add_subdirectory(libs/core)
add_subdirectory(libs/http)
add_subdirectory(libs/streaming)
add_subdirectory(apps/server)