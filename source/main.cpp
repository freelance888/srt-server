#include "../headers/main.h"
#include <csignal>

pthread_t self;

void terminateHandler(int signum) {
    cout << "Received signal: " << signum << endl;
    if (enable_rtmp) {
        pthread_kill(rtmp_thread, SIGKILL);
    }
    pthread_cancel(connthread_infos[0].thread);
    pthread_cancel(transferthread_infos[0].thread);

    exit(signum);
}

int main(int argc, char *argv[]) {
    self = pthread_self();
    signal(SIGINT, terminateHandler);
    signal(SIGTERM, terminateHandler);
//    signal(SIGKILL, terminateHandler);

    srt_startup();
    srt_setloglevel(srt_logging::LogLevel::fatal);

    int yes = 1, no = 0;

    service_rcv = string("9000");
    service_snd = string("9001");

    if (argc >= 3) {
        if (strcmp(argv[1], argv[2]) == 0) {
            cout << "Argument 1 cannot be equal to argument 2" << endl;
            return 1;
        }
        service_rcv = argv[1];
        service_snd = argv[2];
        if (argc > 3) {
            service_rtmp = argv[3];
            enable_rtmp = true;
            cout << "RTMP input mode is enabled" << endl;
        }
    }

    // Open sockets
    SRTSOCKET sfd_rcv = create_starter_socket(&service_rcv);
    if (sfd_rcv == SRT_INVALID_SOCK) {
        cout << "sfd_rcv starter srt_socket: " << srt_getlasterror_str() << endl;
        return 2;
    }
    SRTSOCKET sfd_snd = create_starter_socket(&service_snd);
    if (sfd_snd == SRT_INVALID_SOCK) {
        cout << "sfd_snd starter srt_socket: " << srt_getlasterror_str() << endl;
        return 2;
    }

    cout << "Server is ready at ports: [LISTEN:" << service_rcv << "] [SEND:" << service_snd << "]" << endl;

    // Starting listen
    if (srt_listen(sfd_rcv, 100) == SRT_ERROR) {
        cout << "srt_listen: " << srt_getlasterror_str() << endl;
        return 3;
    }
    if (srt_listen(sfd_snd, 1000) == SRT_ERROR) {
        cout << "srt_listen: " << srt_getlasterror_str() << endl;
        return 3;
    }

    // Create epolls
    int epid_rcv = srt_epoll_create();
    if (epid_rcv < 0) {
        cout << "srt_epoll_create rcv: " << srt_getlasterror_str() << endl;
        return 4;
    }

    int epid_snd = srt_epoll_create();
    if (epid_snd < 0) {
        cout << "srt_epoll_create snd: " << srt_getlasterror_str() << endl;
        return 4;
    }

    if (srt_epoll_add_usock(epid_rcv, sfd_rcv, &EVENTS_RCV) == SRT_ERROR) {
        cout << "srt_epoll_add_usock rcv: " << srt_getlasterror_str() << endl;
        return 5;
    }

    if (srt_epoll_add_usock(epid_snd, sfd_snd, &EVENTS_SND) == SRT_ERROR) {
        cout << "srt_epoll_add_usock snd: " << srt_getlasterror_str() << endl;
        return 5;
    }


    char data[1500];
    int data_len = 0;

    if (pthread_mutex_init(&lock, NULL) != 0) {
        printf("Mutex (lock) init failed\n");
        return 6;
    }
    if (pthread_mutex_init(&log_lock, NULL) != 0) {
        printf("Mutex (log_lock) init failed\n");
        return 6;
    }
    if (pthread_mutex_init(&stat_lock, NULL) != 0) {
        printf("Mutex (stat_lock) init failed\n");
        return 6;
    }

    init_log();

    // Start connection thread
    connthread_infos[0].is_used = true;
    connthread_infos[0].msg_buff = (char *) malloc(WORKTHREAD_MSG_BUFF_LEN);
    used_conn_threads++;
    connectionsthread_info connthread_0_info{
            .epollin=epid_rcv,
            .epollout=epid_snd,
            .basesockin=sfd_rcv,
            .basesockout=sfd_snd,
            .thread_info=&connthread_infos[0]
    };

    if (pthread_create(&connthread_infos[0].thread, NULL, connections_handler, (void *) (&connthread_0_info)) != 0) {
        cout << "cannot create connection thread 0: " << strerror(errno) << endl;
        return 7;
    }

    connthread_infos[0].is_alive = true;

    // Start transfer thread
    transferthread_infos[0].is_used = true;
    transferthread_infos[0].msg_buff = (char *) malloc(WORKTHREAD_MSG_BUFF_LEN);
    used_transfer_treads++;
    datatransferthread_info transferthread_0_info{
            .epollin=epid_rcv,
            .epollout=epid_snd,
            .basesockin=sfd_rcv,
            .basesockout=sfd_snd,
            .thread_info=&transferthread_infos[0]
    };

    if (pthread_create(&transferthread_infos[0].thread, nullptr, handle_data_transfer,
                       (void *) (&transferthread_0_info)) != 0) {
        cout << "cannot create transfer thread 0: " << strerror(errno) << endl;
        return 7;
    }

    if (enable_rtmp) {
        if (pthread_create(&rtmp_thread, nullptr, begin_rtmp, nullptr) != 0) {
            cout << "cannot create rtmp thread" << strerror(errno) << endl;
            return 7;
        }
    }

    transferthread_infos[0].is_alive = true;

    log(LOG_INFO, (char *) "Server was successfully initialized\n");

    uint64_t last_ms = get_current_ms();

    // the event loop
    while (true) {

        this_thread::sleep_for(milliseconds(100));
        if (get_current_ms() - last_ms >= 1000) {

            pthread_mutex_lock(&stat_lock);

            float timedelta = (float) (get_current_ms() - last_ms) / 1000;
            total_rcv_bytes += temp_rcv_bytes;
            total_send_bytes += temp_send_bytes;
            print_stats(timedelta);
            temp_rcv_bytes = 0;
            temp_send_bytes = 0;

            pthread_mutex_unlock(&stat_lock);

            last_ms = get_current_ms();
        }

        // monitor transfer threads
        for (int i = 0; i < used_transfer_treads; ++i) {
            workerthread_info thread_info = transferthread_infos[i];

            // If for some reason the thread was marked as 'unused'
            if (!thread_info.is_used) {
                snprintf(mainthreadmsg_buff, sizeof(mainthreadmsg_buff),
                         "[MAINTHREAD] !!! %d transfer threads used but %dth of them was set 'unused', resetting\n",
                         used_transfer_treads,
                         i);
                log(LOG_WARNING, mainthreadmsg_buff);
                thread_info.is_used = true;
            }

            // If it is used but not alive
            if (!thread_info.is_alive) {
                snprintf(mainthreadmsg_buff, sizeof(mainthreadmsg_buff),
                         "[MAINTHREAD] transfer thread %d was closed unexpectedly. Reason: %s\n", i,
                         thread_info.msg_buff);
                log(LOG_WARNING, mainthreadmsg_buff);

                if (pthread_create(&transferthread_infos[0].thread, NULL, handle_data_transfer,
                                   (void *) (&transferthread_0_info)) != 0) {
                    snprintf(mainthreadmsg_buff, sizeof(mainthreadmsg_buff),
                             "[MAINTHREAD] cannot recreate transfer thread %d: %s; finishing...\n", i, strerror(errno));
                    log(LOG_ERR, mainthreadmsg_buff);
                    exit(9);
                }

                snprintf(mainthreadmsg_buff, sizeof(mainthreadmsg_buff),
                         "[MAINTHREAD] transfer thread %d was successfully recreated\n", i);
                log(LOG_WARNING, mainthreadmsg_buff);

                thread_info.is_alive = true;
            }
        }

        // monitor connection threads
        for (int i = 0; i < used_conn_threads; ++i) {
            workerthread_info thread_info = connthread_infos[i];

            // If for some reason the thread was marked as 'unused'
            if (!thread_info.is_used) {
                snprintf(mainthreadmsg_buff, sizeof(mainthreadmsg_buff),
                         "[MAINTHREAD] !!! %d connections threads used but %dth of them was set 'unused', resetting\n",
                         used_transfer_treads,
                         i);
                log(LOG_WARNING, mainthreadmsg_buff);
                thread_info.is_used = true;
            }

            // If it is used but not alive
            if (!thread_info.is_alive) {
                snprintf(mainthreadmsg_buff, sizeof(mainthreadmsg_buff),
                         "[MAINTHREAD] connection thread %d was closed unexpectedly. Reason: %s\n", i,
                         thread_info.msg_buff);
                log(LOG_WARNING, mainthreadmsg_buff);

                if (pthread_create(&connthread_infos[0].thread, NULL, handle_data_transfer,
                                   (void *) (&connthread_0_info)) != 0) {
                    snprintf(mainthreadmsg_buff, sizeof(mainthreadmsg_buff),
                             "[MAINTHREAD] cannot recreate connection thread %d: %s; finishing...\n", i,
                             strerror(errno));
                    log(LOG_ERR, mainthreadmsg_buff);
                    exit(9);
                }

                snprintf(mainthreadmsg_buff, sizeof(mainthreadmsg_buff),
                         "[MAINTHREAD] connection thread %d was successfully recreated\n", i);
                log(LOG_WARNING, mainthreadmsg_buff);

                thread_info.is_alive = true;
            }
        }
    }

    srt_close(sfd_rcv);
    srt_close(sfd_snd);
    srt_cleanup();

    pthread_mutex_destroy(&lock);

    return 0;
}

FILE *fFfmpeg;

void *begin_rtmp(void *opinfo) {
    char command[1000];
    snprintf(command, 1000,
             "ffmpeg -fflags +genpts -listen 1 -re -i rtmp://0.0.0.0:%s/rtmp/rtmp2srt "
             "-acodec copy -vcodec copy -strict -2 -y -f mpegts srt://127.0.0.1:%s?pkt_size=1316&latency=2000000",
             service_rtmp.c_str(), service_rcv.c_str());

    while (true) {
        cout << "Start ffmpeg" << endl;
        system(command);
        cout << "FFmpeg is closed, restarting in 1 second..." << endl;
        sleep(1);
    }

    return 0;
}

SRTSOCKET create_starter_socket(string *service) {
    addrinfo hints;
    addrinfo *res;

    int yes = 1, no = 0;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    if (getaddrinfo(nullptr, service->c_str(), &hints, &res) != 0) {
        cout << "illegal port number or port is busy.\n" << endl;
        return 0;
    }

    SRTSOCKET sfd = srt_create_socket();
    if (sfd == SRT_INVALID_SOCK) {
        cout << "srt_socket: " << srt_getlasterror_str() << endl;
        return 0;
    }

    srt_setsockflag(sfd, SRTO_RCVSYN, &no, sizeof no);
    srt_setsockflag(sfd, SRTO_SNDSYN, &no, sizeof no);

    if (srt_bind(sfd, res->ai_addr, res->ai_addrlen == SRT_ERROR)) {
        cout << "srt_bind: " << srt_getlasterror_str() << endl;
        return 0;
    }

    freeaddrinfo(res);

    return sfd;
}
