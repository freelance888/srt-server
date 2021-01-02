#include <iostream>
#include <srt/srt.h>
#include <fstream>
#include <iostream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <netdb.h>
#include <stdexcept>
#include <pthread.h>

using namespace std;

struct dataop_info {
    int epollin;
    int epollout;
    SRTSOCKET basesockin;
    SRTSOCKET basesockout;
    char *msg;
    int *flags;
};

SRTSOCKET create_starter_socket(string *service);

void *handle_data_operations(void *data);

const int srtrfdslenmax = 100;
const int srtsfdslenmax = 1000;

pthread_mutex_t lock;
pthread_t transferthread;
char err_msg_buff[1000];

int main(int argc, char *argv[]) {

    srt_startup();
    srt_setloglevel(srt_logging::LogLevel::fatal);

    int yes = 1, no = 0;

    string service_rcv("9000");
    string service_snd("9001");

    if (argc == 3) {
        if (strcmp(argv[1], argv[2]) == 0) {
            cout << "Argument 1 cannot be equal to argument 2" << endl;
            return 0;
        }
        service_rcv = argv[1];
        service_snd = argv[2];
    }

    // Open sockets
    SRTSOCKET sfd_rcv = create_starter_socket(&service_rcv);
    if (sfd_rcv == SRT_INVALID_SOCK) {
        cout << "sfd_rcv srt_socket: " << srt_getlasterror_str() << endl;
        return 0;
    }
    SRTSOCKET sfd_snd = create_starter_socket(&service_snd);
    if (sfd_snd == SRT_INVALID_SOCK) {
        cout << "sfd_snd srt_socket: " << srt_getlasterror_str() << endl;
        return 0;
    }

    cout << "server is ready at ports: [LISTEN:" << service_rcv << "] [SEND:" << service_snd << "]" << endl;

    // Starting listen
    if (srt_listen(sfd_rcv, 100) == SRT_ERROR) {
        cout << "srt_listen: " << srt_getlasterror_str() << endl;
        return 0;
    }
    if (srt_listen(sfd_snd, 1000) == SRT_ERROR) {
        cout << "srt_listen: " << srt_getlasterror_str() << endl;
        return 0;
    }

    // Create epolls
    int epid_rcv = srt_epoll_create();
    if (epid_rcv < 0) {
        cout << "srt_epoll_create rcv: " << srt_getlasterror_str() << endl;
        return 0;
    }

    int epid_snd = srt_epoll_create();
    if (epid_snd < 0) {
        cout << "srt_epoll_create snd: " << srt_getlasterror_str() << endl;
        return 0;
    }

    int events_rcv = SRT_EPOLL_IN | SRT_EPOLL_ERR;
    int events_rcv_et = SRT_EPOLL_IN | SRT_EPOLL_ERR | SRT_EPOLL_ET;
    if (srt_epoll_add_usock(epid_rcv, sfd_rcv, &events_rcv) == SRT_ERROR) {
        cout << "srt_epoll_add_usock rcv: " << srt_getlasterror_str() << endl;
        return 0;
    }

    int events_snd = SRT_EPOLL_OUT | SRT_EPOLL_ERR;
    int events_snd_et = SRT_EPOLL_OUT | SRT_EPOLL_ERR | SRT_EPOLL_ET;
    if (srt_epoll_add_usock(epid_snd, sfd_snd, &events_snd) == SRT_ERROR) {
        cout << "srt_epoll_add_usock snd: " << srt_getlasterror_str() << endl;
        return 0;
    }

    SRTSOCKET srtrfds[srtrfdslenmax];
    SRTSOCKET srtsfds[srtrfdslenmax];

    int src_count = 0, target_count = 0;
    char data[1500];
    int data_len = 0;

    int thread_completed = 0;
    dataop_info data_ops_info;
    data_ops_info.basesockin = sfd_rcv;
    data_ops_info.basesockout = sfd_snd;
    data_ops_info.epollin = epid_rcv;
    data_ops_info.epollout = epid_snd;
    data_ops_info.flags = &thread_completed;

    if (pthread_mutex_init(&lock, NULL) != 0) {
        printf("mutex init failed\n");
        return 1;
    }
    pthread_create(&transferthread, NULL, handle_data_operations, (void *) (&data_ops_info));
    pthread_detach(transferthread);

    // the event loop
    while (true) {

        // check secondary thread state to be running
        pthread_mutex_lock(&lock);
        if (thread_completed) {
            cout << "[main thread] secondary thread finished with: " << err_msg_buff << ";\nrestarting... " << endl;
            thread_completed = 0;
            pthread_create(&transferthread, NULL, handle_data_operations, (void *) (&data_ops_info));
            pthread_detach(transferthread);
        }
        pthread_mutex_unlock(&lock);

        // Handle SOURCE connections
        int srtrfdslen = srtrfdslenmax;

        // Look for who wants to send data
        int n_rcv = srt_epoll_wait(epid_rcv, &srtrfds[0], &srtrfdslen, 0, 0, 100, 0, 0, 0, 0);
        // assert
        if (n_rcv > srtrfdslenmax) {
            throw std::invalid_argument("epoll rcv returned more than n sockets");
        }

        for (int i = 0; i < n_rcv; i++) {
            SRTSOCKET s_rcv = srtrfds[i];
            SRT_SOCKSTATUS status_rcv = srt_getsockstate(s_rcv);

            if ((status_rcv == SRTS_BROKEN) ||
                (status_rcv == SRTS_NONEXIST) ||
                (status_rcv == SRTS_CLOSED)) {
                cout << "source disconnected. status=" << status_rcv << endl;
                srt_close(s_rcv);
                src_count = max(src_count - 1, 0);
                continue;
            } else if (s_rcv == sfd_rcv) { // Need to accept a new connection?
                // assert
                if (status_rcv != SRTS_LISTENING) {
                    throw std::invalid_argument("status != SRTS_LISTENING");
                }

                SRTSOCKET fhandle;
                sockaddr_storage clientaddr;
                int addrlen = sizeof(clientaddr);

                if (src_count > 0) {
                    continue;
                }

                fhandle = srt_accept(sfd_rcv, (sockaddr *) &clientaddr, &addrlen);
                if (fhandle == SRT_INVALID_SOCK) {
                    cout << "rcv srt_accept: " << srt_getlasterror_str() << endl;
                    return 0;
                }

                char clienthost[NI_MAXHOST];
                char clientservice[NI_MAXSERV];
                getnameinfo((sockaddr *) &clientaddr, addrlen,
                            clienthost, sizeof(clienthost),
                            clientservice, sizeof(clientservice), NI_NUMERICHOST | NI_NUMERICSERV);

                cout << "New source: " << clienthost << ":" << clientservice << endl;
                if (srt_epoll_add_usock(epid_rcv, fhandle, &events_rcv) == SRT_ERROR) {
                    cout << "rcv srt_epoll_add_usock: " << srt_getlasterror_str() << endl;
                    return 0;
                }
                src_count++;
            }
        }

        // Handle TARGET connections
        int srtsfdslen = srtsfdslenmax;

        // Look for who wants to receive data
        int n_snd = srt_epoll_wait(epid_snd, 0, 0, &srtsfds[0], &srtsfdslen, 100, 0, 0, 0, 0);
        // assert
        if (n_rcv > srtrfdslenmax) {
            throw std::invalid_argument("epoll rcv returned more than n sockets");
        }

        for (int i = 0; i < n_snd; i++) {
            SRTSOCKET s_snd = srtsfds[i];
            SRT_SOCKSTATUS status_snd = srt_getsockstate(s_snd);

            if ((status_snd == SRTS_BROKEN) ||
                (status_snd == SRTS_NONEXIST) ||
                (status_snd == SRTS_CLOSED)) {
                cout << "target disconnected. status=" << status_snd << endl;
                srt_close(s_snd);
                target_count = max(target_count - 1, 0);
                continue;
            } else if (s_snd == sfd_snd) { // Need to accept a new connection?
                // assert
                if (status_snd != SRTS_LISTENING) {
                    throw std::invalid_argument("status != SRTS_LISTENING");
                }

                SRTSOCKET fhandle;
                sockaddr_storage clientaddr;
                int addrlen = sizeof(clientaddr);

                if (target_count >= srtsfdslenmax) {
                    continue;
                }

                fhandle = srt_accept(sfd_snd, (sockaddr *) &clientaddr, &addrlen);
                if (fhandle == SRT_INVALID_SOCK) {
//                    cout << "snd srt_accept: " << srt_getlasterror_str() << endl;
//                    goto end;
                    continue;
                }

                char clienthost[NI_MAXHOST];
                char clientservice[NI_MAXSERV];
                getnameinfo((sockaddr *) &clientaddr, addrlen,
                            clienthost, sizeof(clienthost),
                            clientservice, sizeof(clientservice), NI_NUMERICHOST | NI_NUMERICSERV);

                cout << "New target: " << clienthost << ":" << clientservice << endl;
                if (srt_epoll_add_usock(epid_snd, fhandle, &events_snd) == SRT_ERROR) {
                    cout << "snd srt_epoll_add_usock: " << srt_getlasterror_str() << endl;
                    return 0;
                }
                target_count++;
            }
        }
    }

    end:
    if (!thread_completed) {
        pthread_cancel(transferthread);
    }
    srt_close(sfd_rcv);
    srt_close(sfd_snd);
    srt_cleanup();

    pthread_mutex_destroy(&lock);

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

void *handle_data_operations(void *opinfo) {
    dataop_info *info = (dataop_info *) opinfo;
    int epid_rcv = info->epollin;
    int epid_snd = info->epollout;
    SRTSOCKET basesockin = info->basesockin;
    SRTSOCKET basesockout = info->basesockout;

    SRTSOCKET srtrfds[srtrfdslenmax];
    SRTSOCKET srtsfds[srtrfdslenmax];

    char data[1500];
    int data_len = 0;

    while (true) {
        int srtrfdslen = srtrfdslenmax;
        int srtsfdslen = srtsfdslenmax;

        // Look for who wants to send data
        int n_rcv = srt_epoll_wait(epid_rcv, &srtrfds[0], &srtrfdslen, 0, 0, 100, 0, 0, 0, 0);
        // assert
        if (n_rcv > srtrfdslenmax) {
            snprintf(err_msg_buff, sizeof(err_msg_buff),
                     "[transmit thread] epoll rcv returned more than n sockets");
            cout << err_msg_buff << endl;
            goto end;
        }

        for (int i = 0; i < n_rcv; i++) {
            SRTSOCKET s_rcv = srtrfds[i];
            SRT_SOCKSTATUS status_rcv = srt_getsockstate(s_rcv);

            if ((status_rcv == SRTS_BROKEN) ||
                (status_rcv == SRTS_NONEXIST) ||
                (status_rcv == SRTS_CLOSED)) {
                // Socket opening/closing is handled within the main thread
                continue;
            } else if (s_rcv == basesockin) {
                continue;
            } else {
                // While we have data to extract
                // Package size by default is 1316 bytes
                while (true) {
                    // read
                    data_len = srt_recvmsg(s_rcv, data, sizeof(data));

                    if (data_len == SRT_ERROR) {
                        // EAGAIN for SRT READING
                        int last_err = srt_getlasterror(nullptr);
                        if (last_err != SRT_EASYNCRCV) {
                            snprintf(err_msg_buff, sizeof(err_msg_buff), "[transmit thread] srt_recvmsg: %s",
                                     srt_getlasterror_str());
                            cout << err_msg_buff << endl;
//                            goto end;
                        }
                        break;
                    }

                    if (data_len == 0 || data_len == SRT_ERROR) {
                        break;
                    }

                    // look for who is able to receive the data
                    int n_snd = srt_epoll_wait(epid_snd, 0, 0, &srtsfds[0], &srtsfdslen, 0, 0, 0, 0, 0);
                    if (n_snd > srtsfdslenmax) {
                        snprintf(err_msg_buff, sizeof(err_msg_buff),
                                 "[transmit thread] epoll snd returned more than n sockets");
                        cout << err_msg_buff << " " << n_snd << "/" << srtsfdslen << endl;
                        goto end;
                    }

                    // iterate every client which is open to receive the data
                    for (int j = 0; j < n_snd; ++j) {
                        SRTSOCKET s_snd = srtsfds[j];
                        if (s_snd == basesockout) {
                            // base socket is needed only for accepting new connections
                            continue;
                        }

                        SRT_SOCKSTATUS status_snd = srt_getsockstate(s_snd);
                        if ((status_snd == SRTS_BROKEN) ||
                            (status_snd == SRTS_NONEXIST) ||
                            (status_snd == SRTS_CLOSED)) {
                            // Socket opening/closing is handled within the main thread
                            continue;
                        } else {
                            // send
                            srt_sendmsg(s_snd, data, data_len, -1, true);
                        }
                    }
                    // flush buffer
                    data_len = 0;
                }
            }
        }
    }

    end:
    pthread_mutex_lock(&lock);
    *info->flags = 1;
    pthread_mutex_unlock(&lock);
    return 0;
}
