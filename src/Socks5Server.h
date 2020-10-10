//
// Created by danny liu on 10/7/2020.
//

#ifndef SOCKS5SERVER_SOCKS5SERVER_H
#define SOCKS5SERVER_SOCKS5SERVER_H
#include <iostream>
#include<stdio.h>
#include <iostream>
#include<mutex>
#include<queue>
#include<unistd.h>
#include<cstring>
#include<mutex>
#include<unistd.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<fcntl.h>
#include<cassert>
#include<thread>
#include<sys/epoll.h>
#include<list>
#include<set>
#include<map>
#include<event2/dns.h>
#include<event.h>   //use libevent
#include<sys/eventfd.h>

#include"Protocol.h"
#include"FifoBuffer.h"
#include"ServerUtil.h"
#include"ServerConfigs.h"

#define CON_STAT_CONNECTED      0
#define CON_STAT_AUTH           1
#define CON_STAT_CMD            2
#define CON_STAT_GET_IP         3
#define CON_STAT_CONNECTING     4
#define CON_STAT_COMMUNICATING  5
#define CON_STAT_CLOSE          -1

#define UPCON_STAT_CONNECTING   0
#define UPCON_STAT_CONNECTED    1

#define WORKER_SIG_STOP         1
#define WORKER_SIG_CON_NUM      2
#define MIN_BUFFER_SIZE         512

//configs;
struct ServerErr :std::exception
{
    std::string why;
    ServerErr(const std::string& why)
    {
        this -> why = why;
    }
    ServerErr()
    {
        this->why = strerror(errno);
    }
    const char * what() const noexcept override
    {
        return why.c_str();
    }
};
typedef int32_t CONNECTION_STAT;
struct Connection
{
    Connection(int fd,int upCon,sockaddr_in& address);
    void bufferIn(char *data,int dataLen);
    int                         fd;
    int                         upCon;
    AUTH_METHOD                 authMethod;
    ServerUtil::FifoBuffer<100> snd;
    sockaddr_in                 address;
    sockaddr_in                 dst;
    std::vector<char>           buffer;
    event                       readEvent;
    event                       writeEvent;
    event                       ttlTimer;
    CONNECTION_STAT             stat;
};

struct DnsReq
{
    explicit DnsReq(int clientFd,const Client_CMD&cmd);
    int clientFd;
    Client_CMD req;
};

struct UpConn
{
    UpConn(int fd,int clientCon,sockaddr_in &address);
    int                         fd;
    int                         clientCon;
    int32_t                     stat;
    sockaddr_in                 address;
    ServerUtil::FifoBuffer<100> snd;
    event                       readEvent;
    event                       writeEvent;
};

struct WorkerData
{
    event_base *                eventBase;
    evdns_base *                evdnsBase;
    event*                      serverSig;
    event                       listenEv;
    std::map<int , Connection*> connections;
    std::map<int , UpConn*>     upcons;
    char *                      buffer;
};
class Socks5Server
{
public:
    Socks5Server();
    static std::map<std::string ,std::string > id_key;
    void init(int threadN,int port,const char * ip=nullptr);
    void start();
    void stop();
    void printConNum();
    std::list<std::thread*> threads;
    static thread_local WorkerData workerData;
    static void onNewCon(int fd,short events,void *args);           //listen fd event
    //client connection events
    static void onClientReadEvent(int fd,short events,void *args);
    static void onClientWriteEvent(int fd,short events,void *args);
    static void onClientTimerEvent(int fd,short events,void *args);
    //connection of dst server events;
    static void onUpConReadEvent(int fd,short events,void* args);
    static void onUpConWriteEvent(int fd,short events,void* args);

    static void onNewServerSig(int fd,short events,void *args);     //event of server control;

    static void closeClient(int fd);                                //close a client completely (close up connection and client connection)
    static void closeUpServer(int fd);                              //close connection of dst server ,does nothing with client connection

    static void sendToClient(int fd,const char *data,int length);   //send data to client throught fifobUffer;
    static void sendToUpServer(int fd,const unsigned char *data,int length);//send data to dst server through fifobuffer;

    static char choseAuthMethod(std::vector<unsigned char>& supported);
    static int  connectUpServer(Connection&clientCon,sockaddr_in &address);

    static CONNECTION_STAT connectedStatShift(Connection &con,char *recved,int length);
    static CONNECTION_STAT authStatShift(Connection &con,char *recved,int length);
    static CONNECTION_STAT cmdStatShift(Connection &con,char *recved,int length);
    static CONNECTION_STAT communicatingStatShift(Connection &con,char *recved,int length);

    static void addEvent(event *event,event_base *eventBase);
    static int lookup_host(const char * host,DnsReq*req);

    static void dns_callback(int errcode, struct evutil_addrinfo *addr, void *ptr);
    static void resetClientTTL(int fd,int32_t time);

private:
    void            threadLoop(int evfd);
    std::list<int>  evfds;
    static void     workerInit(int evfd,int listenfd);
    static void     workerLoopExit();
    static void     workerExitClean();
    sockaddr_in     address;
    int             listenFd;
    int             threadN;
};


#endif //SOCKS5SERVER_SOCKS5SERVER_H
