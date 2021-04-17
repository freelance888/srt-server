//
// Created by amukhsimov on 17/4/21.
//

#ifndef SRT_SERVER_SRTINSTANCE_H
#define SRT_SERVER_SRTINSTANCE_H

#include <srt/srt.h>
#include <string.h>
#include <vector>
#include <stdexcept>
#include <map>

#include "srtconfig.h"
#include "source.h"

using namespace std;

#define SRT_INVALID_ROOM -1

// TODO: add/remove redundant/needed fields
typedef struct {
    SRTSOCKET fd;
    int roomid;
    sockaddr clntaddr;
    bool israndezvouz;
    int callerdir;
    int transferdir;
} SRTCONNINFO;

typedef struct {
    int fd;
    int hs_version;
    string streamid;
} SRTSOCKINFO;

class SrtRoom {
public:
    SrtRoom(string streamid);

    int initRoom();

    int closeRoom();

    string getStreamId() { return this->m_streamid; }

    int addConnection(SRTCONNINFO conn);

    int removeConnection(SRTCONNINFO conn);

    int removeConnection(SRTSOCKET fd);

    void setSource(Source *source);

    int serve();

    void broadcast(char *data, int size, void *arg = nullptr);

private:
    int checksource();

private:
    string m_streamid;

    Source *m_source;
    map<SRTSOCKET, SRTCONNINFO> m_connections;

    SRTSOCKET m_srtrfds[SRT_MAX_ROOM_CONNECTIONS];
    SRTSOCKET m_srtwfds[SRT_MAX_ROOM_CONNECTIONS];
    char m_databuffer[1500];

    int m_epid_rcv, m_epid_snd;
};

class SrtInstance {
public:
    // Constructor/destructor
    SrtInstance(string service);

    ~SrtInstance();

    // Interface
    int init();

    int cleanup();

    int addRoom(string streamid);

    int addConnection(SRTCONNINFO conn);

    void handleConnections();

    void dataTransfer();

    int setSocketRoom(SRTSOCKET fd, SRTSOCKINFO sockinfo);

    // Properties
    SRTSOCKET getListenFd() { return this->m_fd; }

    int getEpollId() { return this->m_epid; }

    SrtRoom *getRoom(string streamid) { return this->m_rooms[streamid]; }

    map<string, SrtRoom *> getRooms() { return this->m_rooms; }

private:

    string m_service;
    SRTSOCKET m_fd;
    int m_epid;
    SRTSOCKET m_srtfds[SRT_MAX_INSTANCE_ROOMS];
    map<string, SrtRoom *> m_rooms;
    map<SRTSOCKET, SRTSOCKINFO> m_socketsinfo;
};

int srtCbListenFn(void *opaque, SRTSOCKET ns, int hs_version, const struct sockaddr *peeraddr, const char *streamid);

void srtBroadcast(char *buffer, int bufflen, void *arg);

#endif //SRT_SERVER_SRTINSTANCE_H
