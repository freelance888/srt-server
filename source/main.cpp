#include "../headers/main.h"
#include <csignal>

string service_srt, service_rtmp;
bool enable_rtmp;
pthread_t rtmp_thread;

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

    if (enable_rtmp) {
        if (pthread_create(&rtmp_thread, nullptr, begin_rtmp, nullptr) != 0) {
            cout << "Cannot create rtmp thread: " << strerror(errno) << endl;
            return -1;
        }
    }

    cout << "Server is ready on port: " << service_srt << endl;

    while (true) {
        instance->handleConnections();
        instance->dataTransfer();
        usleep(10000);
    }

    return 0;
}

void *begin_rtmp(void *opinfo) {
    char command[1000];
    snprintf(command, 1000,
             "ffmpeg -fflags +genpts -listen 1 -re -i rtmp://0.0.0.0:%s/rtmp/rtmp2srt "
             "-acodec copy -vcodec copy -strict -2 -y -f mpegts srt://127.0.0.1:%s?pkt_size=1316&latency=2000000",
             service_rtmp.c_str(), service_srt.c_str());

    while (true) {
        cout << "Start ffmpeg" << endl;
        system(command);
        cout << "FFmpeg is closed, restarting in 1 second..." << endl;
        sleep(1);
    }

    return 0;
}
