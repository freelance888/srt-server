CMAKE_MINIMUM_REQUIRED(VERSION 3.16)
PROJECT(srt-server)

SET(CMAKE_CXX_STANDARD 11)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

ADD_EXECUTABLE(srt-server headers/main.h source/main.cpp headers/srtconfig.h headers/srtinstance.h headers/source.h source/source.cpp source/srtinstance.cpp source/srtroom.cpp)
TARGET_LINK_LIBRARIES(srt-server -lsrt)

INSTALL(TARGETS srt-server)
