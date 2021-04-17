//
// Created by amukhsimov on 17/4/21.
//

#ifndef SRT_SERVER_SOURCE_H
#define SRT_SERVER_SOURCE_H

#include <srt/srt.h>
#include <string.h>
#include <vector>
#include <stdexcept>
#include <map>
#include <iostream>

#include "srtconfig.h"

using namespace std;

typedef void(*onDataAvailableCallback)(char *, int, void *);

typedef struct {
    onDataAvailableCallback callback;
    void *arg;
} CallbackStructure;

class Source {
public:
    virtual ~Source() = 0;

    virtual int isValid() = 0;

    virtual int fetchNext() = 0;

    virtual char *getBuffer() = 0;

    virtual int getFd() = 0;

    virtual void subscribe(onDataAvailableCallback callback, void *arg = nullptr) = 0;
};

class SrtSource : public Source {
public:
    SrtSource(SRTSOCKET fd, char *initialData = nullptr, int size = 0);

    ~SrtSource() {
        delete this->m_buffer;
        srt_close(this->m_fd);
    }

    int isValid() override;

    char *getBuffer() override { return this->m_buffer; }

    int getFd() override { return this->m_fd; }

    int fetchNext() override;

    void subscribe(onDataAvailableCallback callback, void *arg = nullptr) override {
        this->m_callbacks.push_back(CallbackStructure{callback = callback, arg = arg});
    }

private:
    SRTSOCKET m_fd;
    int m_epid;
    char *m_buffer;
    int m_bufflen;
    bool m_initialBuffer;
    SRTSOCKET m_srtrfds[SRT_MAX_ROOM_CONNECTIONS];
    vector<CallbackStructure> m_callbacks;
};

#endif //SRT_SERVER_SOURCE_H
