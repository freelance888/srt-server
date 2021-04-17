#include "../headers/source.h"


Source::~Source() = default;

SrtSource::SrtSource(SRTSOCKET fd, char *initialData, int size) {
    this->m_fd = fd;
    this->m_bufflen = 0;
    this->m_buffer = new char[SRT_BUFFER_SIZE];
    this->m_initialBuffer = false;

    if (initialData && size > 0) {
        this->m_initialBuffer = true;
        memcpy(this->m_buffer, initialData, size);
        this->m_bufflen = size;
    }

    this->m_epid = srt_epoll_create();

    if (this->m_epid < 0) {
        cout << "SRT: SrtSource::SrtSource(): srt_epoll_create(): " << srt_getlasterror_str() << endl;
        // TODO: log error
    }

    int events = SRT_EPOLL_IN | SRT_EPOLL_ERR;
    if (srt_epoll_add_usock(this->m_epid, this->m_fd, &events) == SRT_ERROR) {
        cout << "SRT: SrtSource::SrtSource(): srt_epoll_add_usock(): " << srt_getlasterror_str() << endl;
        // TODO: log error
    }
}

int SrtSource::isValid() {
    SRT_SOCKSTATUS rstatus = srt_getsockstate(this->m_fd);

    // If socket is invalid
    if (rstatus == SRTS_BROKEN ||
        rstatus == SRTS_NONEXIST ||
        rstatus == SRTS_CLOSED) {
        return 0;
    }
    return 1;
}

int SrtSource::fetchNext() {
    if (this->m_initialBuffer) {
        for (auto it: this->m_callbacks) {
            it.callback(this->m_buffer, this->m_bufflen, it.arg);
        }
        this->m_initialBuffer = false;
        return this->m_bufflen;
    }

    int srtrfdslen = SRT_MAX_ROOM_CONNECTIONS;
    // Note that since we only poll ONE socket into m_epid, nr should be wether 0 or 1
    int nr = srt_epoll_wait(this->m_epid, this->m_srtrfds, &srtrfdslen, 0, 0, 0, 0, 0, 0, 0);

    this->m_bufflen = 0;

    for (int ir = 0; ir < nr; ++ir) {
        SRTSOCKET rfd = this->m_srtrfds[ir];
        SRT_SOCKSTATUS rstatus = srt_getsockstate(rfd);

        if (rfd != this->m_fd) {
            // TODO: log
            cout << "SRT: SrtSource::fetchNext(): unknown socket polled in m_epid" << endl;
            continue;
        }

        // If socket is invalid
        if (rstatus == SRTS_BROKEN ||
            rstatus == SRTS_NONEXIST ||
            rstatus == SRTS_CLOSED) {
            return SRT_ERROR;
        }

        this->m_bufflen = srt_recvmsg(rfd, this->m_buffer, SRT_BUFFER_SIZE);
        if (this->m_bufflen == SRT_ERROR) {
            if (srt_getlasterror(nullptr) != SRT_EASYNCRCV) {
                // TODO: log
                cout << "SRT: SrtSource::fetchNext(): srt_recvmsg(): " << srt_getlasterror_str() << endl;
                return SRT_ERROR;
            }
        }
        break;
    }

    if (this->m_bufflen > 0) {
        for (auto it: this->m_callbacks) {
            it.callback(this->m_buffer, this->m_bufflen, it.arg);
        }
    }

    return this->m_bufflen;
}
