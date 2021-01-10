CMAKE_MINIMUM_REQUIRED(VERSION 3.16)
PROJECT(srt-server)

SET(CMAKE_CXX_STANDARD 11)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

ADD_EXECUTABLE(srt-server headers/structs.h headers/main.h source/core.cpp source/main.cpp source/environment.cpp)
TARGET_LINK_LIBRARIES(srt-server -lsrt)

INSTALL(TARGETS srt-server)
