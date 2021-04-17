#include "../headers/main.h"
#include <csignal>

string service_srt, service_rtmp;
bool enable_rtmp;

int main(int argc, char *argv[]) {
//    self = pthread_self();
//    signal(SIGINT, terminateHandler);
//    signal(SIGTERM, terminateHandler);
//    signal(SIGKILL, terminateHandler);

    service_srt = string("9000");

    if (argc >= 2) {
        service_srt = argv[1];
        if (argc > 2) {
            service_rtmp = argv[2];
            enable_rtmp = true;
            cout << "RTMP input mode is enabled" << endl;
        }
    }

    SrtInstance *instance = new SrtInstance(service_srt);
    int err = instance->init();
    if (err != 0) {
        cout << "SRT instance initialization error: " << err << endl;
        return -1;
    }

    cout << "Server is ready on port: " << service_srt << endl;

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
