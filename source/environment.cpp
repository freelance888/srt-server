#include "../headers/main.h"

// Source/target
bool enable_rtmp = false;
int src_count = 0;
int target_count = 0;
list<SRTSOCKET> sockets_in, sockets_out;
string service_rcv, service_snd, service_rtmp;

// Statistics
int64_t total_rcv_bytes = 0, total_send_bytes = 0;
int64_t temp_rcv_bytes = 0, temp_send_bytes = 0;

// Thread control shared variables
pthread_mutex_t lock, log_lock, stat_lock;
int used_transfer_treads = 0;
int used_conn_threads = 0;
workerthread_info transferthread_infos[MAX_TRANSFER_THREADS];
workerthread_info connthread_infos[MAX_CONN_THREADS];
pthread_t rtmp_thread;

// Temp buffers
char mainthreadmsg_buff[WORKTHREAD_MSG_BUFF_LEN];
char tempbuff[TEMP_BUFF_SIZE];
char timestr[50];

// Logging
ofstream *fcout;
