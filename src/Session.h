//
// Created by danny on 12/3/2020.
//

#ifndef SOCKS5_SESSION_H
#define SOCKS5_SESSION_H

#include"FifoBuffer.h"
#include"Protocol.h"
#include<event.h>
#include<evdns.h>
enum class ConStat
{
    NEGOTIATE = 0,
    AUTH = 1,
    CMD = 2,
    CONNECTING_DST = 3,
    COMMUNICATE = 4,
    WAIT = 5
};

class Worker;
struct Session
{
    Session(int a_id,Worker *worker);
    void assignB(int b_id);
    void bufferIn(char * data,int len);
    void resetTimer(int sec);
    void close();
    int a_id;
    int b_id;
    Worker * worker;
    AUTH_METHOD authMethod;
    std::string id;
    std::vector<char> protoBuffer;
    ServerUtil::FifoBuffer<500> a_buffer;
    ServerUtil::FifoBuffer<500> b_buffer;
    evdns_getaddrinfo_request* dnsReq;
    ConStat stat;
    Client_CMD cmd;
    event aRead;
    event aWrite;
    event bRead;
    event bWrite;
    event timer;
};


#endif //SOCKS5_SESSION_H
