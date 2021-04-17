#include "../headers/srtinstance.h"


SrtRoom::SrtRoom(string streamid) {
    this->m_streamid = streamid;
}

int SrtRoom::initRoom() {
    this->m_source = nullptr;
    this->m_epid_rcv = srt_epoll_create();

    if (this->m_epid_rcv < 0) {
        cout << "srt_epoll_create: " << srt_getlasterror_str() << endl;
        return SRT_ERROR;
        // TODO: log error
    }

    this->m_epid_snd = srt_epoll_create();
    if (this->m_epid_snd < 0) {
        cout << "srt_epoll_create: " << srt_getlasterror_str() << endl;
        return SRT_ERROR;
        // TODO: log error
    }

    return 0;
}

int SrtRoom::closeRoom() {
    throw std::logic_error{"Not implemented exception: SrtRoom::closeRoom()"};
    return 0;
}

int SrtRoom::addConnection(SRTCONNINFO conn) {
    if (this->m_connections.find(conn.fd) == this->m_connections.end()) {
        if (this->m_connections.size() == SRT_MAX_ROOM_CONNECTIONS) {
            // TODO: log
            cout << "SrtRoom::addConnection max room connections exceeded" << endl;
            return SRT_ERROR;
        }
        this->m_connections[conn.fd] = conn;

        int events = SRT_EPOLL_IN | SRT_EPOLL_ERR;
        if (srt_epoll_add_usock(this->m_epid_rcv, conn.fd, &events) == SRT_ERROR) {
            cout << "srt_epoll_add_usock rcv: " << srt_getlasterror_str() << endl;
            return 6;
        }

        events = SRT_EPOLL_OUT | SRT_EPOLL_ERR;
        if (srt_epoll_add_usock(this->m_epid_snd, conn.fd, &events) == SRT_ERROR) {
            cout << "srt_epoll_add_usock snd: " << srt_getlasterror_str() << endl;
            return 6;
        }

        return 0;
    }
    return SRT_ERROR;
}

int SrtRoom::removeConnection(SRTCONNINFO conn) {
    return this->removeConnection(conn.fd);
}

int SrtRoom::removeConnection(SRTSOCKET fd) {
    auto search = this->m_connections.find(fd);
    if (search != this->m_connections.end()) {
        srt_close(fd);
        this->m_connections.erase(fd);
        return 0;
    }
    return SRT_ERROR;
}

int SrtRoom::serve() {

    // Return, if there is no connections in the room
    int conncnt = this->m_connections.size();
    if (conncnt == 0) {
        return 0;
    }

    if (!this->checksource()) {
        return 0;
    }

    // Since on SrtRoom::setSource we subscribe to the source and provide a callback for broadcasting,
    // here is no need to broadcast again
    while (this->m_source->isValid() && this->m_source->fetchNext() > 0);

    return 0;
}

void SrtRoom::broadcast(char *data, int size, void *arg) {
    int srtwfdslen = SRT_MAX_ROOM_CONNECTIONS;
    int nw = srt_epoll_wait(this->m_epid_snd, 0, 0, this->m_srtwfds, &srtwfdslen, 0, 0, 0, 0, 0);

    for (int iw = 0; iw < nw; ++iw) {
        SRTSOCKET wfd = this->m_srtwfds[iw];
        SRT_SOCKSTATUS wstatus = srt_getsockstate(wfd);

        // If a socket is the source, do not echo the data
        if (wfd == this->m_source->getFd()) {
            continue;
        }
        // If socket is invalid
        if (wstatus == SRTS_BROKEN ||
            wstatus == SRTS_NONEXIST ||
            wstatus == SRTS_CLOSED) {
            // TODO: log
            this->removeConnection(wfd);
            cout << "Target disconnected" << endl;
        }

        srt_sendmsg(wfd, data, size, -1, true);
    }
}

int SrtRoom::checksource() {

    if (this->m_source) {
        if (this->m_source->isValid()) {
            return 1;
        } else {
            // TODO: close source, log
            this->removeConnection(this->m_source->getFd());
            delete this->m_source;
            this->m_source = nullptr;
        }
    }

    int srtrfdslen = SRT_MAX_ROOM_CONNECTIONS;
    int nr = srt_epoll_wait(this->m_epid_rcv, this->m_srtrfds, &srtrfdslen, 0, 0, 0, 0, 0, 0, 0);

    for (int ir = 0; ir < nr; ++ir) {
        SRTSOCKET rfd = this->m_srtrfds[ir];
        SRT_SOCKSTATUS rstatus = srt_getsockstate(rfd);

        // If socket is invalid
        if (rstatus == SRTS_BROKEN ||
            rstatus == SRTS_NONEXIST ||
            rstatus == SRTS_CLOSED) {
            cout << "Closing socket..." << endl;
            this->removeConnection(rfd);
            continue;
        }

        // Consider rfd is a source
        // TODO: checkout if old packets still remain in poll
        int ret = srt_recvmsg(rfd, this->m_databuffer, sizeof(this->m_databuffer));
        if (ret == SRT_ERROR) {
            if (srt_getlasterror(nullptr) != SRT_EASYNCRCV) {
                // TODO: log
                cout << "srt_recvmsg: " << srt_getlasterror_str() << endl;
                return SRT_ERROR;
            }
            break;
        }

        if (ret >= 800) {
            auto *source = new SrtSource(rfd, this->m_databuffer, ret);
            this->setSource(source);
            return 1;
        }
    }
    return 0;
}

void SrtRoom::setSource(Source *source) {
    if (this->m_source != source) {
        this->m_source = source;
        this->m_source->subscribe(srtBroadcast, this);
    }
}

void srtBroadcast(char *buffer, int bufflen, void *arg) {
    auto *room = (SrtRoom *) arg;
    room->broadcast(buffer, bufflen, arg);
}
