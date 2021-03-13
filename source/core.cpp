#include "../headers/main.h"

#define SET_BYTES_SIZING(bytes_total, bytes_var, suff_var) \
        if (bytes_total <= 1024) { \
            bytes_var = (float)bytes_total; \
            suff_var = (char *)"B"; \
        } else if (bytes_total <= 1024 * 1024) { \
            bytes_var = (float)bytes_total / 1024.0f; \
            suff_var = (char *)"KB"; \
        } else if (bytes_total <= pow(1024, 3)) { \
            bytes_var = (float)bytes_total / pow(1024.0f, 2); \
            suff_var = (char *)"MB"; \
        } else { \
            bytes_var = (float)bytes_total / pow(1024.0f, 3); \
            suff_var = (char *)"GB"; \
        }

uint64_t get_current_ms() {
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

void set_thread_closed(workerthread_info *thread_info, char *msg, int loglevel, bool set_closemsg = false) {
    thread_info->is_alive = false;
    log(loglevel, msg);
    if (set_closemsg && thread_info->msg_buff != msg) {
        strcpy(thread_info->msg_buff, msg);
    }
}

void *connections_handler(void *ptr) {

    connectionsthread_info *info = (connectionsthread_info *) ptr;
    if (!info->thread_info->msg_buff) {
        pthread_mutex_lock(&lock);
        info->thread_info->msg_buff = (char *) malloc(WORKTHREAD_MSG_BUFF_LEN);
        pthread_mutex_unlock(&lock);
    }

    SRTSOCKET srtrcvfds[SRT_RCV_SOCK_MAX_LEN];
    SRTSOCKET srtsndfds[SRT_SND_SOCK_MAX_LEN];

    while (true) {

        // Handle SOURCE connections
        int srtrcvfdslen = SRT_RCV_SOCK_MAX_LEN;

        // Look for who wants to send data
        int n_rcv = srt_epoll_wait(info->epollin, &srtrcvfds[0], &srtrcvfdslen, 0, 0, 100, 0, 0, 0, 0);

        for (int i = 0; i < n_rcv; i++) {
            SRTSOCKET sock_rcv = srtrcvfds[i];
            SRT_SOCKSTATUS status_rcv = srt_getsockstate(sock_rcv);

            if ((status_rcv == SRTS_BROKEN) ||
                (status_rcv == SRTS_NONEXIST) ||
                (status_rcv == SRTS_CLOSED)) {
                // Close socket, remove from list and log
                rm_socket(sock_rcv, SRT_INVALID_SOCK);
                continue;
            } else if (sock_rcv == info->basesockin) { // Need to accept a new connection?
                // assert
                if (status_rcv != SRTS_LISTENING) {
                    // Set thread closed and log error
                    set_thread_closed(info->thread_info,
                                      (char *) "[CONNTHREAD] basesockin->status != SRTS_LISTENING, stopping thread...\n",
                                      LOG_ERR, true);
                    return nullptr;
                }

                SRTSOCKET fhandle;
                sockaddr_storage clientaddr;
                int addrlen = sizeof(clientaddr);

                // Restriction: only one source accepted for now
                if (src_count > 0) {
                    continue;
                }

                fhandle = srt_accept(sock_rcv, (sockaddr *) &clientaddr, &addrlen);
                if (fhandle == SRT_INVALID_SOCK) {
                    continue;
                }
                SRT_SOCKSTATUS status_fd = srt_getsockstate(fhandle);
                if ((status_fd == SRTS_BROKEN) ||
                    (status_fd == SRTS_NONEXIST) ||
                    (status_fd == SRTS_CLOSED)) {
                    srt_close(fhandle);
                    continue;
                }

                int srtt = SRTT_LIVE;
                srt_setsockflag(fhandle, SRTO_TRANSTYPE, &srtt, sizeof(srtt));

                char clienthost[NI_MAXHOST];
                char clientservice[NI_MAXSERV];
                getnameinfo((sockaddr *) &clientaddr, addrlen,
                            clienthost, sizeof(clienthost),
                            clientservice, sizeof(clientservice), NI_NUMERICHOST | NI_NUMERICSERV);

                snprintf(info->thread_info->msg_buff, WORKTHREAD_MSG_BUFF_LEN, "[CONNTHREAD] New source: %s:%s\n",
                         clienthost, clientservice);
                log(LOG_INFO, info->thread_info->msg_buff);

                if (srt_epoll_add_usock(info->epollin, fhandle, &EVENTS_RCV) == SRT_ERROR) {
                    // Set error message
                    snprintf(info->thread_info->msg_buff, WORKTHREAD_MSG_BUFF_LEN,
                             "[CONNTHREAD] rcv srt_epoll_add_usock: %s; stopping thread...\n",
                             srt_getlasterror_str());
                    // Set thread closed and log error
                    set_thread_closed(info->thread_info, info->thread_info->msg_buff, LOG_ERR);
                    return nullptr;
                }
                add_socket(fhandle, SRT_INVALID_SOCK);
            }
        }

        // Handle TARGET connections
        int srtsndfdslen = SRT_SND_SOCK_MAX_LEN;

        // Look for who wants to receive data
        int n_snd = srt_epoll_wait(info->epollout, 0, 0, &srtsndfds[0], &srtsndfdslen, 50, 0, 0, 0, 0);

        for (int i = 0; i < n_snd; i++) {
            SRTSOCKET sock_snd = srtsndfds[i];
            SRT_SOCKSTATUS status_snd = srt_getsockstate(sock_snd);

            if ((status_snd == SRTS_BROKEN) ||
                (status_snd == SRTS_NONEXIST) ||
                (status_snd == SRTS_CLOSED)) {
                // Close socket, remove from list and log
                rm_socket(SRT_INVALID_SOCK, sock_snd);
                continue;
            } else if (sock_snd == info->basesockout) { // Need to accept a new connection?
                // assert
                if (status_snd != SRTS_LISTENING) {
                    // Set thread closed and log error
                    set_thread_closed(info->thread_info,
                                      (char *) "[CONNTHREAD] basesockout->status != SRTS_LISTENING, stopping thread...\n",
                                      LOG_ERR, true);
                    return nullptr;
                }

                SRTSOCKET fhandle;
                sockaddr_storage clientaddr;
                int addrlen = sizeof(clientaddr);

                if (target_count >= SRT_SND_SOCK_MAX_LEN) {
                    continue;
                }

                fhandle = srt_accept(sock_snd, (sockaddr *) &clientaddr, &addrlen);
                if (fhandle == SRT_INVALID_SOCK) {
                    continue;
                }
                SRT_SOCKSTATUS status_fd = srt_getsockstate(fhandle);
                if ((status_fd == SRTS_BROKEN) ||
                    (status_fd == SRTS_NONEXIST) ||
                    (status_fd == SRTS_CLOSED)) {
                    srt_close(fhandle);
                    continue;
                }

                int srtt = SRTT_LIVE;
                srt_setsockflag(fhandle, SRTO_TRANSTYPE, &srtt, sizeof(srtt));

                char clienthost[NI_MAXHOST];
                char clientservice[NI_MAXSERV];
                getnameinfo((sockaddr *) &clientaddr, addrlen,
                            clienthost, sizeof(clienthost),
                            clientservice, sizeof(clientservice), NI_NUMERICHOST | NI_NUMERICSERV);

                snprintf(info->thread_info->msg_buff, WORKTHREAD_MSG_BUFF_LEN, "[CONNTHREAD] New target: %s:%s\n",
                         clienthost, clientservice);
                log(LOG_INFO, info->thread_info->msg_buff);

                if (srt_epoll_add_usock(info->epollout, fhandle, &EVENTS_SND) == SRT_ERROR) {
                    // Set error message
                    snprintf(info->thread_info->msg_buff, WORKTHREAD_MSG_BUFF_LEN,
                             "[CONNTHREAD] snd srt_epoll_add_usock: %s; stopping thread...\n",
                             srt_getlasterror_str());
                    // Set thread closed and log error
                    set_thread_closed(info->thread_info, info->thread_info->msg_buff, LOG_ERR);
                    return nullptr;
                }
                add_socket(SRT_INVALID_SOCK, fhandle);
            }
        }
    }
}

void *handle_data_transfer(void *opinfo) {
    datatransferthread_info *info = (datatransferthread_info *) opinfo;
    if (!info->thread_info->msg_buff)
        info->thread_info->msg_buff = (char *) malloc(WORKTHREAD_MSG_BUFF_LEN);
    int epid_rcv = info->epollin;
    int epid_snd = info->epollout;
    SRTSOCKET basesockin = info->basesockin;
    SRTSOCKET basesockout = info->basesockout;

    SRTSOCKET srtrcvfds[SRT_RCV_SOCK_MAX_LEN];
    SRTSOCKET srtsndfds[SRT_SND_SOCK_MAX_LEN];

    char data[1500];
    int data_len = 0;

    while (true) {
        int srtrcvfdslen = SRT_RCV_SOCK_MAX_LEN;
        int srtsndfdslen = SRT_SND_SOCK_MAX_LEN;

        // Look for who wants to send data
        int n_rcv = srt_epoll_wait(epid_rcv, &srtrcvfds[0], &srtrcvfdslen, 0, 0, 100, 0, 0, 0, 0);

        for (int i = 0; i < n_rcv; i++) {
            SRTSOCKET sock_rcv = srtrcvfds[i];
            SRT_SOCKSTATUS status_rcv = srt_getsockstate(sock_rcv);

            if ((status_rcv == SRTS_BROKEN) ||
                (status_rcv == SRTS_NONEXIST) ||
                (status_rcv == SRTS_CLOSED)) {
                // Socket opening/closing is handled within the connections thread
                continue;
            } else if (sock_rcv == basesockin) {
                continue;
            } else {
                // While we have data to extract
                // Package size by default is 1316 bytes
                while (true) {
                    // read
                    data_len = srt_recvmsg(sock_rcv, data, sizeof(data));

                    if (data_len == SRT_ERROR) {
                        // EAGAIN for SRT READING
                        int last_err = srt_getlasterror(nullptr);
                        if (last_err != SRT_EASYNCRCV) {
                            snprintf(info->thread_info->msg_buff, WORKTHREAD_MSG_BUFF_LEN,
                                     "[transmit thread] srt_recvmsg: %s\n",
                                     srt_getlasterror_str());
                            log(LOG_WARNING, info->thread_info->msg_buff);
                        }
                        break;
                    }

                    if (data_len == 0) {
                        break;
                    }

                    pthread_mutex_lock(&stat_lock);
                    temp_rcv_bytes += data_len;
                    pthread_mutex_unlock(&stat_lock);

                    // look for who is able to receive the data
                    int n_snd = srt_epoll_wait(epid_snd, 0, 0, &srtsndfds[0], &srtsndfdslen, 0, 0, 0, 0, 0);
                    if (n_snd > SRT_SND_SOCK_MAX_LEN) {
                        snprintf(info->thread_info->msg_buff, WORKTHREAD_MSG_BUFF_LEN,
                                 "[transmit thread] epoll snd returned more than %d sockets\n", SRT_SND_SOCK_MAX_LEN);
                        set_thread_closed(info->thread_info, info->thread_info->msg_buff, LOG_ERR);
                        return nullptr;
                    }

                    // iterate every client which is open to receive the data
                    for (int j = 0; j < n_snd; ++j) {
                        SRTSOCKET sock_snd = srtsndfds[j];
                        if (sock_snd == basesockout) {
                            // base socket is needed only for accepting new connections
                            continue;
                        }

                        SRT_SOCKSTATUS status_snd = srt_getsockstate(sock_snd);
                        if ((status_snd == SRTS_BROKEN) ||
                            (status_snd == SRTS_NONEXIST) ||
                            (status_snd == SRTS_CLOSED)) {
                            // Socket opening/closing is handled within the connections thread
                            continue;
                        } else {
                            // send
                            srt_sendmsg(sock_snd, data, data_len, -1, true);
                            pthread_mutex_lock(&stat_lock);
                            temp_send_bytes += data_len;
                            pthread_mutex_unlock(&stat_lock);
                        }
                    }
                    // flush buffer
                    data_len = 0;
                }
            }
        }
    }
}

void add_socket(SRTSOCKET s_in, SRTSOCKET s_out) {
    if (s_in != SRT_INVALID_SOCK) {
        pthread_mutex_lock(&lock);
        if (find(sockets_in.begin(), sockets_in.end(), s_in) == sockets_in.end()) {
            sockets_in.push_back(s_in);
            src_count++;
        }
        pthread_mutex_unlock(&lock);
    }
    if (s_out != SRT_INVALID_SOCK) {
        pthread_mutex_lock(&lock);
        if (find(sockets_out.begin(), sockets_out.end(), s_out) == sockets_out.end()) {
            sockets_out.push_back(s_out);
            target_count++;
        }
        pthread_mutex_unlock(&lock);
    }
}

void rm_socket(SRTSOCKET s_in, SRTSOCKET s_out) {
    if (s_in != SRT_INVALID_SOCK) {
        pthread_mutex_lock(&lock);
        if (find(sockets_in.begin(), sockets_in.end(), s_in) != sockets_in.end()) {
            sockets_in.remove(s_in);
            src_count--;
            srt_close(s_in);

            log(LOG_INFO, (char *) "[CONNTHREAD] source has disconnected\n");
        }
        pthread_mutex_unlock(&lock);
    }
    if (s_out != SRT_INVALID_SOCK) {
        pthread_mutex_lock(&lock);
        if (find(sockets_out.begin(), sockets_out.end(), s_out) != sockets_out.end()) {
            sockets_out.remove(s_out);
            target_count--;
            srt_close(s_out);

            log(LOG_INFO, (char *) "[CONNTHREAD] target has disconnected\n");
        }
        pthread_mutex_unlock(&lock);
    }
}

char *get_time_str(int formatlvl) {
    time_t now = time(nullptr);
    tm *p_tm = gmtime(&now);

    switch (formatlvl) {
        case TIMESTR_UALL:
            strftime(timestr, sizeof timestr, "%Y.%m.%d %H:%M:%S", p_tm);
            break;
        case TIMESTR_UDATE:
            strftime(timestr, sizeof timestr, "%Y.%m.%d", p_tm);
            break;
        case TIMESTR_UTIME:
            strftime(timestr, sizeof timestr, "%H:%M:%S", p_tm);
            break;
        case TIMESTR_SALL:
            strftime(timestr, sizeof timestr, "%Y%m%d %H%M%S", p_tm);
            break;
        case TIMESTR_SDATE:
            strftime(timestr, sizeof timestr, "%Y%m%d", p_tm);
            break;
        case TIMESTR_STIME:
            strftime(timestr, sizeof timestr, "%H%M%S", p_tm);
            break;
        default:
            snprintf(timestr, sizeof timestr, "TIME_INVALID_FORMAT");
            break;
    }

    return timestr;
}

char *get_time_formatted(char *format) {
    time_t now = time(nullptr);
    tm *p_tm = gmtime(&now);

    strftime(timestr, sizeof timestr, format, p_tm);

    return timestr;
}

void init_log() {
    char usrname[100];
    getlogin_r(usrname, 100);

    snprintf(tempbuff, sizeof(tempbuff), "/home/%s/.local/share/srt-server/%s_%sto%s.log",
             usrname,
             get_time_formatted("%m%d_%H%M%S"),
             service_rcv.c_str(),
             service_snd.c_str()
    );
    system("mkdir -p ~/.local/share/srt-server");
    fcout = new ofstream(tempbuff);
}

void log(int level, char *msg) {

    switch (level) {
        case LOG_DEBUG:
            snprintf(tempbuff, sizeof(tempbuff), "[DEBUG] ");
            break;
        case LOG_WARNING:
            snprintf(tempbuff, sizeof(tempbuff), "[WARNING] ");
            break;
        case LOG_ERR:
            snprintf(tempbuff, sizeof(tempbuff), "[ERR] ");
            break;
        case LOG_INFO:
        case LOG_CONSOLE:
            snprintf(tempbuff, sizeof(tempbuff), "");
            break;
        default:
            snprintf(tempbuff, sizeof(tempbuff), "[LVL%d] ", level);
            break;
    }

    pthread_mutex_lock(&log_lock);

    // '\033[2K (^[[2K) - is a VT100 escape code which means 'Clear entire line'
    // The list of VT100 escape codes: https://espterm.github.io/docs/VT100%20escape%20codes.html
    cout << "\033[2K\r[" << get_time_str() << "] " << tempbuff << msg << flush;
    if (fcout && fcout->good() && fcout->is_open() && level != LOG_CONSOLE) {
        // But we do not need to put escape code out to file
        (*fcout) << "[" << get_time_str() << "] " << tempbuff << msg << flush;
    }
    pthread_mutex_unlock(&log_lock);
}

void print_stats(float timedelta) {

    float total_rcv = 0, total_snd = 0, temp_rcv = 0, temp_snd;
    char *total_rcv_suff = nullptr, *total_snd_suff = nullptr, *temp_rcv_suff = nullptr, *temp_snd_suff = nullptr;

    // SET_BYTES_SIZING is a macro which transforms, for example millions of bytes into MB, GB etc.
    SET_BYTES_SIZING(total_rcv_bytes, total_rcv, total_rcv_suff)
    SET_BYTES_SIZING(total_send_bytes, total_snd, total_snd_suff)
    SET_BYTES_SIZING(temp_rcv_bytes, temp_rcv, temp_rcv_suff)
    SET_BYTES_SIZING(temp_send_bytes, temp_snd, temp_snd_suff)

    // Stats format
    snprintf(mainthreadmsg_buff, sizeof mainthreadmsg_buff, "[STATS] [%d sources; %d targets] "
                                                            "[+%.1fs] [total rcv: %.1f%s (+%.1f%s)] [total sent: %.1f%s (+%.1f%s)]",
             src_count, target_count, timedelta,
             total_rcv, total_rcv_suff,
             temp_rcv, temp_rcv_suff,
             total_snd, total_snd_suff,
             temp_snd, temp_snd_suff
    );
    log(LOG_CONSOLE, mainthreadmsg_buff);
}
