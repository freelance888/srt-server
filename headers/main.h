//
// Created by akm2109 on 2021-01-03.
//

#ifndef SRT_SERVER_MAIN_H
#define SRT_SERVER_MAIN_H

#include <iostream>
#include <srt/srt.h>
#include <fstream>
#include <iostream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <netdb.h>
#include <stdexcept>
#include <list>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <unistd.h>
#include <thread>
#include <cmath>
#include <ctime>
#include <filesystem>
#include "structs.h"

using namespace std;
using namespace std::chrono;

#define LOG_CONSOLE LOG_LOCAL0
#define TIMESTR_UALL 0
#define TIMESTR_UDATE 1
#define TIMESTR_UTIME 2
#define TIMESTR_SALL 3
#define TIMESTR_SDATE 4
#define TIMESTR_STIME 5

// Sockets counts
const int SRT_RCV_SOCK_MAX_LEN = 100;
const int SRT_SND_SOCK_MAX_LEN = 1000;
// Threads counts
const int MAX_TRANSFER_THREADS = 1;
const int MAX_CONN_THREADS = 1;

// Event types
const int EVENTS_RCV = SRT_EPOLL_IN | SRT_EPOLL_ERR;
const int EVENTS_RCV_ET = SRT_EPOLL_IN | SRT_EPOLL_ERR | SRT_EPOLL_ET;
const int EVENTS_SND = SRT_EPOLL_OUT | SRT_EPOLL_ERR;
const int EVENTS_SND_ET = SRT_EPOLL_OUT | SRT_EPOLL_ERR | SRT_EPOLL_ET;

// Source/target
extern bool enable_rtmp;
extern int src_count;
extern int target_count;
extern list<SRTSOCKET> sockets_in, sockets_out;
extern string service_rcv, service_snd, service_rtmp;

// Statistics
extern int64_t total_rcv_bytes, total_send_bytes;
extern int64_t temp_rcv_bytes, temp_send_bytes;

// Thread control shared variables
extern pthread_mutex_t lock, log_lock, stat_lock;
extern int used_transfer_treads;
extern int used_conn_threads;
extern workerthread_info transferthread_infos[MAX_TRANSFER_THREADS];
extern workerthread_info connthread_infos[MAX_CONN_THREADS];
extern pthread_t rtmp_thread;

// Temp buffers
extern char mainthreadmsg_buff[WORKTHREAD_MSG_BUFF_LEN];
extern char tempbuff[TEMP_BUFF_SIZE];
extern char timestr[50];

// Logging
extern ofstream *fcout;


uint64_t get_current_ms();

SRTSOCKET create_starter_socket(string *service);

void *begin_rtmp(void *opinfo);

void *handle_data_transfer(void *opinfo);

void *connections_handler(void *ptr);

void add_socket(SRTSOCKET s_in, SRTSOCKET s_out);

void rm_socket(SRTSOCKET s_in, SRTSOCKET s_out);

// Accessory:

char *get_time_formatted(char *format);

char *get_time_str(int formatlvl = TIMESTR_UALL);

void init_log();

void log(int level, char *msg);

void print_stats(float timedelta);

#endif //SRT_SERVER_MAIN_H
