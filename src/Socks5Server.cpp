//
// Created by danny on 11/30/2020.
//

#include "Socks5Server.h"

#include<signal.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<fcntl.h>
#include<evdns.h>
//#include<iostream> // for debug
#include<sys/eventfd.h>

#include"ServerUtil.h"
#include"config_parser/ConfigParser.h"
#include"Protocol.h"


void Socks5Server::setConfig(std::string path)
{
    this->configPath = path;

}

void Socks5Server::init()
{
    ServerUtil::BlockSigno(SIGPIPE);
    this->cfg.readConfig(this->configPath);
    this->listenFd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if(this->listenFd<0)
    {
        throw ServerErr();
    }
    sockaddr_in address;
    address.sin_port = htons(cfg.PORT);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(cfg.SERVER_IP.c_str());
    if(bind(listenFd,(sockaddr*)&address,sizeof(address))<0)
    {
        close(listenFd);
        throw ServerErr();
    }
    if(listen(listenFd,1024)<0)
    {
        close(listenFd);
        throw ServerErr();
    }
    if(ServerUtil::setNonblock(listenFd)<0)
    {
        close(listenFd);
        throw ServerErr();
    }
    for(int i = 0;i<this->cfg.THREADS;i++)
    {
        this->workers.push_back(new Worker(&this->cfg,this->listenFd));
    }
}

void Socks5Server::start()
{
    for(auto x: this->workers)
    {
        x->start();
    }

}

void Socks5Server::stop()
{
    for(auto &x : this->workers)
    {
        x->interrupt();
        delete x;
    }
    workers.clear();

}

std::vector<int> Socks5Server::getSessionLoad() {
    std::vector<int> ret;
    for(auto x : this->workers)
    {
        ret.push_back(x->stat_getSessionNum());
    }
    return ret;
}

int Socks5Server::getOnlineWorkerNum()
{
    return this->workers.size();
}

Worker::Worker(const ServerConfig *cfg, int listenFd):listenFd(listenFd),cfg(cfg),stat_sessionNum(0)
{
    buffer = new char[cfg->SEND_UNIT_SIZE + 10];
    eventBase = event_init();
    evdnsBase = evdns_base_new(eventBase,1);

    event_set(&listenEv,listenFd,EV_READ|EV_PERSIST,onNewCon,this);
    event_base_set(eventBase,&listenEv);
    event_add(&listenEv,NULL);

    this->outerSignalFd = eventfd(0,EFD_NONBLOCK);
    if(outerSignalFd<0)
        throw ServerErr();
    event_set(&this->sigEvent,this->outerSignalFd,EV_PERSIST|EV_READ,Worker::onOuterSig,this);
    event_base_set(this->eventBase,&this->sigEvent);
    event_add(&this->sigEvent,NULL);
}

void Worker::start()
{
    this->workerThread = new std::thread(&Worker::threadLoop,this);
}

void Worker::threadLoop()
{
    event_base_dispatch(this->eventBase);
}

void Worker::onNewCon(int fd, short events, void *_self)
{
    Worker * self = (Worker*)_self;
    sockaddr_in address;
    socklen_t len = sizeof(address);
    int newFd=accept(fd,(sockaddr*)&address,&len);
    if(newFd<0)
    {
        if(errno==EAGAIN)
        {
            return;
        }
        //todo :error accept new fd <0;
        return;
    }
    Session* session = new Session(newFd,self);
    self->addEvent(&session->aRead,NULL);
    self->addEvent(&session->timer,self->cfg->CLIENT_TTL);
    self->sessions.insert(session);
    session->resetTimer(self->cfg->CLIENT_TTL);
    //statistic refresh;
    self->stat_sessionNum++;
}

void Worker::addEvent(event *event,int timeoutSec)
{
    timeval time;
    time.tv_sec = timeoutSec;
    if(timeoutSec)
        event_add(event,&time);
    else
        event_add(event,NULL);

}

void Worker::onARead(int fd, short events, void *session_)
{
    Session * session = (Session*)session_;
    const ServerConfig* cfg = session->worker->cfg;
    Worker * worker = session->worker;
    char *buffer = worker->buffer;
    errno = 0;
    int readLen = read(fd,buffer,cfg->SEND_UNIT_SIZE);
    if(readLen<=0&&errno!=EAGAIN)
    {
        //error or disconnected;
        worker->onAClose(session);
        return;
    }else if(errno==EAGAIN)
    {
        worker->addEvent(&session->aRead,NULL);
        return;
    }
    session->resetTimer(worker->cfg->CLIENT_TTL);
    switch (session->stat)
    {
        case ConStat::NEGOTIATE:
            worker->onNegotiateStatShift(buffer,readLen,session);
            break;
        case ConStat::AUTH:
            worker->onAuthStatShift(buffer,readLen,session);
            break;
        case ConStat::CMD:
            worker->onCmdStatShift(buffer,readLen,session);
            break;
        case ConStat::COMMUNICATE:
            worker->onCommunicateStatShift(buffer,readLen,0,session);
            break;
    }
}

void Worker::onAWrite(int fd, short events, void *session_)
{
    Session * session = (Session*)session_;
    Worker *worker = session->worker;
    const ServerConfig *cfg = worker->cfg;
    int sendLen = session->a_buffer.writeOut(fd,cfg->SEND_UNIT_SIZE);
    if(sendLen<0)
    {
        //error
        worker->onAClose(session);
    }
    else if(session->a_buffer.empty())
    {
        //no data to be send;
        worker->onABufferNoData(session);
        return;
    }else if(errno == EAGAIN)
    {
        worker->addEvent(&session->aWrite,NULL);
    }
}

void Worker::onBRead(int fd, short events, void *session_)
{
    Session * session = (Session*)session_;
    const ServerConfig* cfg = session->worker->cfg;
    Worker * worker = session->worker;
    char *buffer = worker->buffer;
    errno = 0;
    int readLen = read(fd,buffer,cfg->SEND_UNIT_SIZE);
    if(readLen<=0&&errno!=EAGAIN)
    {
        //error or disconnected;
        worker->onBClose(session);
        return;
    }else if(errno==EAGAIN)
    {
        worker->addEvent(&session->bRead,NULL);
        return;
    }
    session->resetTimer(cfg->CLIENT_TTL);
    worker->onCommunicateStatShift(buffer,readLen,1,session);
}

void Worker::onBWrite(int fd, short events, void *session_)
{
    Session *session = (Session * ) session_;
    Worker *worker = session->worker;
    const ServerConfig* cfg = worker->cfg;
    if(session->stat == ConStat::CONNECTING_DST)
    {
        //check if connection is ready;
        int32_t error = 0;
        socklen_t errorLen = sizeof(error);
        if (getsockopt(fd, SOL_SOCKET, SO_ERROR, (char *)&error, &errorLen) < 0)
        {
            error = 1;
        }
        worker->onConnectingDstStatShift(error?CMD_RESPONSE_TARGET_REFUSED:CMD_RESPONSE_OK,session);
    }
    else
    {
        int sendLen = session->b_buffer.writeOut(fd,cfg->SEND_UNIT_SIZE);
        if(sendLen<0)
        {
            //error
            worker->onBClose(session);
        }
        else if(session->b_buffer.empty())
        {
            //no data to be send;
            worker->onBBufferNoData(session);
            return;
        }else if(errno == EAGAIN)
        {
            worker->addEvent(&session->bWrite,NULL);
        }

    }
}

void Worker::onSessionTimeOut(int fd, short events, void *session_ )
{
    Session *session =(Session*) session_;
    session->worker->closeSession(session);
}

void Worker::stop()
{
    event_del(&this->listenEv);
}


void Worker::dns_cb(int errcode, addrinfo *addr, void *ptr)
{
    if(errcode ==EVUTIL_EAI_CANCEL)// dns request was canceled by closeSession() so ptr is invalid
    {
        return;
    }
    Session *session = (Session*)ptr;
    Worker *worker = session->worker;
    session->dnsReq = nullptr;
    if (errcode)
    {
        //todo : resolve error
        worker->onConnectingDstStatShift(CMD_RESPONSE_TARGET_INVALID,session);
        return;
    }
    else
    {
        struct evutil_addrinfo *ai = addr;
        if (ai->ai_family == AF_INET)
        {
            sockaddr_in addr  = *(struct sockaddr_in *) ai->ai_addr;
            addr.sin_port = session->cmd.port;
            addr.sin_family = AF_INET;
            int conRes = ::connect(session->b_id,(sockaddr*)&addr,sizeof(sockaddr_in));
            if(conRes<0&&errno!=EINPROGRESS)
            {
                //error occurred
                worker->onConnectingDstStatShift(CMD_RESPONSE_TARGET_REFUSED,session);
            }
            else
            {
                event_set(&session->bWrite,session->b_id,EV_WRITE,onBWrite,session);
                worker->addEvent(&session->bWrite,NULL);
            }
        }
        else
        {
            //error
            worker->onConnectingDstStatShift(CMD_RESPONSE_TGT_NOTSPT,session);
        }
        evutil_freeaddrinfo(addr);
    }
}

void Worker::onNegotiateStatShift(char *data, int dataLen, Session *session) {
    session->bufferIn(data,dataLen);
    AuthGetRequest request;
    try
    {
        Protocol::parse((unsigned char *)session->protoBuffer.data(),session->protoBuffer.size(),request);
    } catch (...)
    {
        return;
    }
    session->protoBuffer.clear();
    addEvent(&session->aRead,NULL);
    if(request.version==0x05)
    {
        AuthGetReply authGetReply;
        authGetReply.version = 0x05;
        authGetReply.authMethod = chooseAuth(request.methods);
        session->authMethod = authGetReply.authMethod;
        int len = Protocol::build((unsigned char*)this->buffer,this->cfg->SEND_UNIT_SIZE,authGetReply);
        sendA(session,this->buffer,len);
        session->stat = (session->authMethod == AUTH_NONE? ConStat::CMD:ConStat::AUTH );
    }
    else
    {
        //version wrong

    }
}

AUTH_METHOD Worker::chooseAuth(std::vector<unsigned char> &methods)
{
    for(auto x:methods)
    {
        if(x == AUTH_NONE&&this->cfg->ALLOW_ANONYMOUSE)
        {
            return AUTH_NONE;
        }
        if(x==AUTH_ID_KEY)
        {
            return AUTH_ID_KEY;
        }
    }
    return AUTH_NONE;
}

void Worker::sendA(Session *session, const char *data, int length)
{
    session->a_buffer.writeIn(data,length);
    addEvent(&session->aWrite,NULL);
}


void Worker::onAuthStatShift( char *recved, int length,Session *session)
{
    Worker *worker = session->worker;
    session->bufferIn(recved,length);
    Auth_Id_Key_Req req;
    try {
        Protocol::parse((unsigned char*)recved,length,req);
    }
    catch (...)
    {
        return;
    }
    session->protoBuffer.clear();
    addEvent(&session->aRead,NULL);

    Auth_Id_Key_Rep rep;
    if(session->authMethod==AUTH_ID_KEY)
    {
        rep.version = 0x05;
        rep.authResult = AUTH_RESULT_OK;
        try {
            if(worker->cfg->id_key.at(req.name) != req.passwd)
            {
                throw std::exception();
            }
        } catch (...) {
            rep.authResult = AUTH_RESULT_ERR;
        }
        int sendLen = Protocol::build((unsigned char*)worker->buffer,worker->cfg->SEND_UNIT_SIZE,rep);
        sendA(session,worker->buffer,sendLen);
        session->stat = (rep.authResult==AUTH_RESULT_OK?ConStat::CMD :session->stat);

    }
    else
    {
        //todo : not support yet;
    }
}

void Worker::onCmdStatShift(char *recved, int length, Session *session)
{
    session->bufferIn(recved,length);
    Worker *worker = session->worker;
    Client_CMD req;
    try
    {
        Protocol::parse((unsigned char*)session->protoBuffer.data(),session->protoBuffer.size(),req);
    }
    catch (...)
    {
        return ;
    }
    session->protoBuffer.clear();
    if(req.version!=0x05)
    {
        //error version wrong
        session->stat = ConStat::WAIT;
        Rep_CMD rep(0x05,CMD_RESPONSE_NOT_SUPPORTED,0x00,0x00,{},0x00);
        int sendLen = Protocol::build((unsigned char*)worker->buffer,worker->cfg->SEND_UNIT_SIZE,rep);
        sendA(session,worker->buffer,sendLen);
        return;
    }
    session->cmd = req;
    if(req.cmd==SOCKS_CMD_CONNECT)
    {
        connectBServer(session);
    }
    else
    {
        Rep_CMD rep(0x05,CMD_RESPONSE_NOT_SUPPORTED,0x00,0x00,{},0x00);
        int sendLen = Protocol::build((unsigned char*)worker->buffer,worker->cfg->SEND_UNIT_SIZE,rep);
        sendA(session,worker->buffer,sendLen);
        return;
    }

}

void Worker::onConnectingDstStatShift(int response, Session *session)
{
    Rep_CMD repCmd(0x05,response,0x00,ADDRESS_TYPE_IPV4,{},12345);
    if(response==CMD_RESPONSE_OK)
    {
        //connected;
        addEvent(&session->bRead,NULL);
        addEvent(&session->aRead,NULL);
        session->stat = ConStat::COMMUNICATE;
    }
    else
    {
        //error occurred;
        session->stat = ConStat::WAIT;
    }

    int sendLen = Protocol::build((unsigned char *)this->buffer,cfg->SEND_UNIT_SIZE,repCmd);
    this->sendA(session,this->buffer,sendLen);

}

void Worker::onCommunicateStatShift(char *data, int dataLen, int direction, Session *session_)
{
    Session *session = (Session*)session_;
    Worker* worker = session->worker;
    if(direction)
    {
        //B->A
        sendA(session,data,dataLen);
    }
    else
    {
        //A->B
        sendB(session,data,dataLen);
    }
}

void Worker::sendB(Session *session, const char *data, int length)
{
    session->b_buffer.writeIn(data,length);
    addEvent(&session->bWrite,NULL);
}

void Worker::onABufferNoData(Session *session)
{
    if(session->stat==ConStat::COMMUNICATE)
    {
        addEvent(&session->bRead,NULL);
    }
    if(session->stat==ConStat::WAIT&&session->b_buffer.empty())
    {
        closeSession(session);
    }
}

void Worker::onBBufferNoData(Session *session)
{

    if(session->stat==ConStat::COMMUNICATE)
    {
        addEvent(&session->aRead,NULL);
    }
    if(session->stat==ConStat::WAIT&&session->a_buffer.empty())
    {
        closeSession(session);
    }
}

void Worker::closeSession(Session *session)
{
    event_del(&session->aRead);
    event_del(&session->aWrite);
    event_del(&session->timer);
    if(session->b_id>=0)
    {
        event_del(&session->bWrite);
        event_del(&session->bRead);
    }
    session->close();
    if(session->dnsReq)
    {
        evdns_getaddrinfo_cancel(session->dnsReq);
    }
    delete session;
    this->sessions.erase(session);
    this->stat_sessionNum --;
}

int Worker::connectBServer( Session *session)
{
    auto& req = session->cmd;
    int newFd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    session->assignB(newFd);
    if(session->b_id < 0)
    {
        onConnectingDstStatShift(CMD_RESPONSE_TARGET_INVALID,session);
        return -1;
    }
    ServerUtil::setNonblock(session->b_id);
    switch (req.addr_type)
    {
        case ADDRESS_TYPE_IPV4:
         {
            sockaddr_in addr;
            addr.sin_addr.s_addr = req.addr.ipv4addr;
            addr.sin_port = req.port;
            addr.sin_family = AF_INET;
            session->stat = ConStat::CONNECTING_DST;
            int conRes = ::connect(session->b_id, (sockaddr *) &session->cmd.addr, sizeof(sockaddr_in));
            if (conRes == 0 || (conRes < 0 && errno == EINPROGRESS))
            {
                //connecting
                addEvent(&session->bWrite, NULL);
            } else {
                onConnectingDstStatShift(CMD_RESPONSE_TARGET_INVALID,session);
            }
        }break;
        case ADDRESS_TYPE_DOMAIN:
        {
            struct evutil_addrinfo hints;
            struct evdns_getaddrinfo_request *dns_req;
            memset(&hints, 0, sizeof(hints));
            hints.ai_family = AF_INET;
            hints.ai_flags = EVUTIL_AI_CANONNAME;
            hints.ai_socktype = SOCK_STREAM;
            hints.ai_protocol = IPPROTO_TCP;
            dns_req = evdns_getaddrinfo(evdnsBase,session->cmd.addr.domain,NULL,&hints, dns_cb,session);
            session->dnsReq = dns_req;
            if (dns_req == NULL)
            {
                onConnectingDstStatShift(CMD_RESPONSE_TARGET_INVALID,session);
                return -1;
            }
            session->stat = ConStat::CONNECTING_DST;
            break;
        }
        case ADDRESS_TYPE_IPV6:
            //not support yet;
            onConnectingDstStatShift(CMD_RESPONSE_TGT_NOTSPT,session);
            break;
    }

    return 0;
}

void Worker::onAClose(Session *session)
{
    if(session->stat == ConStat::WAIT)
    {
        closeSession(session);
    }
    else
    {
        session->stat = ConStat::WAIT;
    }
}

void Worker::onBClose(Session *session)
{
    if(session->stat == ConStat::WAIT)
    {
        closeSession(session);
    }
    else
    {
        session->stat = ConStat::WAIT;
    }

}

int Worker::stat_getSessionNum()
{
    return this->stat_sessionNum;
}

void Worker::interrupt()
{
    eventfd_write(this->outerSignalFd,WORKER_SIG_STOP);
    this->workerThread->join();
    for(auto session: this->sessions)
    {
        event_del(&session->aRead);
        event_del(&session->aWrite);
        event_del(&session->timer);
        if(session->b_id>=0)
        {
            event_del(&session->bWrite);
            event_del(&session->bRead);
        }
        session->close();
        if(session->dnsReq)
        {
            evdns_getaddrinfo_cancel(session->dnsReq);
        }
        delete session;
    }
    this->stat_sessionNum = 0;
    this->sessions.clear();
    delete this->workerThread;
    this->workerThread = nullptr;
}

void Worker::onOuterSig(int fd, short events, void *self_)
{
    Worker * self = (Worker*) self_;
    eventfd_t val;
    eventfd_read(fd,&val);
    if(val == WORKER_SIG_STOP)
    {
        event_base_loopexit(self->eventBase, NULL);
        return;
    }
}

Worker::~Worker()
{
    delete buffer;
    event_base_free(eventBase);
    evdns_base_free(evdnsBase,0);
    if(this->outerSignalFd>0)
        close(this->outerSignalFd);
}




