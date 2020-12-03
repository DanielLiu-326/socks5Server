//
// Created by danny on 11/30/2020.
//

#ifndef SOCKS5_SOCKS5SERVER_H
#define SOCKS5_SOCKS5SERVER_H
#include<string>
#include<map>
#include<set>
#include<vector>
#include<string.h>
#include<unistd.h>
#include<netinet/in.h>
#include<event.h>
#include<thread>
#include<list>

#include"ServerConfig.h"
#include"Protocol.h"
#include"FifoBuffer.h"
#include"Session.h"
#define WORKER_SIG_STOP         1

class Worker
{
public:
    friend Session;
    explicit Worker(const ServerConfig* cfg,int listenFd);
    ~Worker();
    void start();
    void interrupt();
    void stop();    //will wait all session be closed;
    //socket layer;
    void static onNewCon(int fd,short events,void *self);//listen fd event
    static void onARead(int fd,short events,void *session);//last arg means this ptr of Worker and session object
    static void onAWrite(int fd,short events,void *session);
    static void onBRead(int fd,short events,void *session);
    static void onBWrite(int fd,short events,void *session);
    static void onOuterSig(int fd,short events,void *self);
    static void onSessionTimeOut(int fd,short events,void *session);//listen fd event
    static void dns_cb(int errcode, evutil_addrinfo *addr, void *ptr);

    //socket layer operation;
    void sendA(Session *session,const char * data,int length);
    void sendB(Session *session,const char * data,int length);
    void closeSession(Session* session);

    void onAClose(Session* session);
    void onBClose(Session* session);
    void onABufferNoData(Session * session);
    void onBBufferNoData(Session * session);
    void onNegotiateStatShift(char * data ,int dataLen , Session* session);

    void onAuthStatShift(char *recved,int length,Session *session);
    void onCmdStatShift(char *recved,int length,Session *session);
    void onConnectingDstStatShift(int error,Session *session);
    void onCommunicateStatShift(char *data,int dataLen,int direction,Session*session_); //direction 0 is a->b 1is b->a

    int connectBServer(Session * session);

    void addEvent(event* event,int timeoutSec);
    AUTH_METHOD chooseAuth(std::vector<unsigned char> & methods);
    //stat functions;
    int stat_getSessionNum();

private:
    int stat_sessionNum;
    int outerSignalFd;
    event sigEvent;
    std::set<Session *> sessions;
    std::thread * workerThread;
    void threadLoop();
    int listenFd;
    const ServerConfig* cfg;
    char * buffer;
    event_base* eventBase;
    evdns_base* evdnsBase;
    event listenEv;

};
class Socks5Server
{
public:
    void setConfig(std::string path);
    void init();
    void start();
    void stop();
    std::vector<int> getSessionLoad();// get connection numbers;
    int getOnlineWorkerNum();

private:
    int listenFd;
    std::list<Worker*> workers;
    std::string configPath;
    ServerConfig cfg;
};


#endif //SOCKS5_SOCKS5SERVER_H
