//
// Created by akm2109 on 2021-01-02.
//

#ifndef SRT_SERVER_STRUCTS_H
#define SRT_SERVER_STRUCTS_H

#include <pthread.h>

const int WORKTHREAD_MSG_BUFF_LEN = 1500;
const int TEMP_BUFF_SIZE = 1500;
struct workerthread_info {
    bool is_used = false;
    pthread_t thread = 0;
    bool is_alive = false;
    char *msg_buff = nullptr;
};
struct common_info {
    int epollin;
    int epollout;
    SRTSOCKET basesockin;
    SRTSOCKET basesockout;
    workerthread_info *thread_info;
};

typedef common_info datatransferthread_info;
typedef common_info connectionsthread_info;

#endif //SRT_SERVER_STRUCTS_H
