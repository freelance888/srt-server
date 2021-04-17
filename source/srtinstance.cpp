#include "../headers/srtinstance.h"

int srtCbListenFn(void *opaque, SRTSOCKET ns, int hs_version, const struct sockaddr *peeraddr, const char *streamid) {
    auto *instance = (SrtInstance *) opaque;

    cout << "srtCbListenFn" << endl;

    SRTSOCKINFO sockinfo;
    sockinfo.fd = ns;
    sockinfo.hs_version = hs_version;
    sockinfo.streamid = string(streamid);

    return instance->setSocketRoom(ns, sockinfo);  // 0 - connection accepted, -1 - reject connection
}

SrtInstance::SrtInstance(string service) {
    this->m_service = service;
    this->m_fd = SRT_INVALID_SOCK;
}

SrtInstance::~SrtInstance() = default;  // TODO: free rooms, etc.

int SrtInstance::init() {
    srt_startup();

    addrinfo hints{};
    addrinfo *res;

    int yes = 1, no = 0;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    if (getaddrinfo(nullptr, this->m_service.c_str(), &hints, &res) != 0) {
        cout << "illegal port number or port is busy.\n" << endl;
        return 1;
    }

    this->m_fd = srt_create_socket();
    if (this->m_fd == SRT_INVALID_SOCK) {
        cout << "srt_socket: " << srt_getlasterror_str() << endl;
        return 2;
    }

    srt_setsockflag(this->m_fd, SRTO_RCVSYN, &no, sizeof no);

    if (srt_bind(this->m_fd, res->ai_addr, res->ai_addrlen) == SRT_ERROR) {
        cout << "srt_bind: " << srt_getlasterror_str() << endl;
        return 3;
    }

    if (srt_listen(this->m_fd, 100) == SRT_ERROR) {
        cout << "srt_listen: " << srt_getlasterror_str() << endl;
        return 4;
    }

    this->m_epid = srt_epoll_create();
    if (this->m_epid < 0) {
        cout << "srt_epoll_create rcv: " << srt_getlasterror_str() << endl;
        return 5;
    }

    int events = SRT_EPOLL_IN | SRT_EPOLL_ERR;
    if (srt_epoll_add_usock(this->m_epid, this->m_fd, &events) == SRT_ERROR) {
        cout << "srt_epoll_add_usock rcv: " << srt_getlasterror_str() << endl;
        return 6;
    }

    freeaddrinfo(res);

    if (srt_listen_callback(this->m_fd, srtCbListenFn, (void *) this) == SRT_ERROR) {
        cout << "srt_listen_callback: " << srt_getlasterror_str() << endl;
        return 7;
    }

    return 0;
}

int SrtInstance::cleanup() {
    srt_cleanup();
    throw std::logic_error{"Not implemented exception: SrtInstance::cleanup()"};
    return 0;
}

void SrtInstance::handleConnections() {
    // TODO: test
    int srtfdslen = 100;
    int n = srt_epoll_wait(this->m_epid, this->m_srtfds, &srtfdslen, 0, 0, 0, 0, 0, 0, 0);

    for (int i = 0; i < n; ++i) {
        SRTSOCKET s = this->m_srtfds[i];
        SRT_SOCKSTATUS status = srt_getsockstate(s);

        if ((status == SRTS_BROKEN) ||
            (status == SRTS_NONEXIST) ||
            (status == SRTS_CLOSED)) {
            // TODO: log
            throw std::logic_error{"Main socket error"};
        }

        if (s != this->m_fd) {
            // TODO: log
            throw std::logic_error{"Error! Unexpected socket polled in main listen epid."};
        }

        if (status != SRTS_LISTENING) {
            // TODO: log
            throw std::logic_error{"Error! Main listen socket is not in listening state."};
        }

        SRTSOCKET fhandle;
        sockaddr clientaddr;
        int addrlen = sizeof(clientaddr);

        cout << "Accepting a new connection..." << endl;
        fhandle = srt_accept(this->m_fd, (sockaddr *) &clientaddr, &addrlen);
        if (SRT_INVALID_SOCK == fhandle) {
            // TODO: log
            return;
        }
        SRT_SOCKSTATUS status_fd = srt_getsockstate(fhandle);
        if ((status_fd == SRTS_BROKEN) ||
            (status_fd == SRTS_NONEXIST) ||
            (status_fd == SRTS_CLOSED)) {
            srt_close(fhandle);
            // TODO: log invalid socket accepted
            return;
        }

        SRTCONNINFO conn;
        conn.fd = fhandle;
        conn.roomid = SRT_INVALID_ROOM;
        conn.clntaddr = clientaddr;
        conn.israndezvouz = false;
        conn.callerdir = DIR_CLIENT_TO_SERVER;
        conn.transferdir = DIR_UNDEFINED;

        if (this->addConnection(conn) == SRT_ERROR) {
            // TODO: log
            cout << "SrtInstance::addConnection error" << endl;
            srt_close(conn.fd);
        }
    }
}

void SrtInstance::dataTransfer() {
    for (const auto& x: this->m_rooms) {
        auto room = x.second;
        room->serve();
    }
}

int SrtInstance::addRoom(string streamid) {
    // TODO: check streamid is valid
    auto room = new SrtRoom(streamid);
    if(room->initRoom() != 0) {
        cout << "SrtInstance::addRoom: SrtRoom::initRoom returned error" << endl;
        return SRT_ERROR;
    }
    this->m_rooms[streamid] = room;

    return 0;
}

int SrtInstance::addConnection(SRTCONNINFO conn) {
    SRTSOCKET fd = conn.fd;

    if (this->m_socketsinfo.find(fd) == this->m_socketsinfo.end()) {
        // TODO: log
        cout << "SrtInstance::addConnection couldn't find socket info" << endl;
        return SRT_ERROR;
    }

    SRTSOCKINFO fdinfo = this->m_socketsinfo[fd];

    if (this->m_rooms.find(fdinfo.streamid) == this->m_rooms.end()) {
        this->addRoom(fdinfo.streamid);
    }

    SrtRoom *room = this->m_rooms[fdinfo.streamid];

    return room->addConnection(conn);
}

int SrtInstance::setSocketRoom(SRTSOCKET fd, SRTSOCKINFO sockinfo) {
    if (sockinfo.hs_version == 4) {
        // TODO: log
        cout << "WARNING: SrtInstance::setSocketRoom: hs_version = 4" << endl;
    }
    if (sockinfo.streamid == "") {
        // TODO: log
        return SRT_ERROR;
    }

    this->m_socketsinfo[fd] = sockinfo;
    return 0;
}
