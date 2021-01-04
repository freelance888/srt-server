cmake_minimum_required(VERSION 3.17)
project(srt_server)

set(CMAKE_CXX_STANDARD 11)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

add_executable(srt_server headers/structs.h headers/main.h source/core.cpp source/main.cpp source/environment.cpp)
target_link_libraries(srt_server -lsrt)

INSTALL(TARGETS srt_server)
