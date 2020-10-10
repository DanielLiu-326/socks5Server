//
// Created by aptx4 on 10/7/2020.
//

#include "Socks5Server.h"
thread_local WorkerData Socks5Server::workerData;
std::map<std::string ,std::string > Socks5Server::id_key;
Socks5Server::Socks5Server()
{


}
void Socks5Server::init(int threadN, int port, const char *ip)
{
    this->threadN = threadN;
    this->listenFd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if(this->listenFd<0)
    {
        throw ServerErr();
    }
    address.sin_port = htons(port);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = ip?inet_addr(ip):INADDR_ANY;
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


}

void Socks5Server::start()
{
    std::cout<<"initing..."<<std::endl;
    std::cout<<"starting "<<this->threadN<<" threads"<<std::endl;
    std::cout<<"ip   : "<<inet_ntoa(this->address.sin_addr)<<std::endl;
    std::cout<<"port : "<<ntohs(this->address.sin_port)<<std::endl;
    for(int i = 0;i<this->threadN;i++)
    {
        int evfd = eventfd(0,EFD_NONBLOCK);
        if(evfd<0)
            throw ServerErr();
        this->evfds.push_back(evfd);
        this->threads.push_back(new std::thread(&Socks5Server::threadLoop,this,evfd));
        usleep(200*1000);
    }
}

void Socks5Server::threadLoop(int evfd)
{
    //all resources store in thread_local struct variable
    // allocate resources needed in event loop
    workerInit(evfd,this->listenFd);
    //event loop start;
    std::cout<<"worker thread : "<<"loop started"<<std::endl;
    event_base_dispatch(workerData.eventBase);
    //clean allocated resources;
    workerExitClean();
}

void Socks5Server::onNewCon(int fd, short events, void *args)
{
    sockaddr_in address;
    socklen_t sockLen = sizeof(address);
    int newfd = accept(fd,(sockaddr*)&address,&sockLen);
    if(newfd<0)
    {
        if(errno==EAGAIN)
        {
            return;
        }
        else
        {
            //todo: errors occurred
        }
    }
    else
    {
        auto newCon = new Connection(newfd,NULL,address);
        workerData.connections[newfd] = newCon;
        addEvent(&newCon->readEvent,workerData.eventBase);
        resetClientTTL(newfd,CLIENT_TTL);
    }
}



Connection::Connection(int fd, int upCon, sockaddr_in &address):
fd(fd),upCon(upCon),address(address),stat(CON_STAT_CONNECTED)
{
    event_set(&this->readEvent,fd,EV_READ,Socks5Server::onClientReadEvent,this);
    event_set(&this->writeEvent,fd,EV_WRITE,Socks5Server::onClientWriteEvent,this);
    evtimer_set(&this->ttlTimer,Socks5Server::onClientTimerEvent,this);
}

void Socks5Server::onClientReadEvent(int fd, short events, void *args)
{
    char *buffer = workerData.buffer;
    auto conData = ((Connection*)args);
    int readLen = read(fd,buffer,SEND_UNIT_SIZE);
    if(readLen<=0&&errno!=EAGAIN)
    {
        //error or disconnected;
        closeClient(fd);
        return;
    }else if(errno==EAGAIN)
    {
        addEvent(&conData->readEvent,workerData.eventBase);
    }
    resetClientTTL(fd,CLIENT_TTL);
    if(conData->stat==CON_STAT_CONNECTED)
    {
        conData->stat = connectedStatShift(*conData,buffer,readLen);
        addEvent(&conData->readEvent,workerData.eventBase);

    }
    else if(conData->stat == CON_STAT_AUTH)
    {
        conData->stat = authStatShift(*conData,buffer,readLen);
        addEvent(&conData->readEvent,workerData.eventBase);
    }
    else if(conData->stat == CON_STAT_CMD)
    {
        conData->stat = cmdStatShift(*conData,buffer,readLen);
        addEvent(&conData->readEvent,workerData.eventBase);
    }
    else if(conData->stat == CON_STAT_GET_IP)
    {
        addEvent(&conData->readEvent,workerData.eventBase);
    }
    else if(conData->stat == CON_STAT_CONNECTING)
    {
        addEvent(&conData->readEvent,workerData.eventBase);
        return;
    }
    else if(conData->stat==CON_STAT_COMMUNICATING)
    {
        conData->stat = communicatingStatShift(*conData,buffer,readLen);
    }

    if(conData->stat==CON_STAT_CLOSE)
    {
        closeClient(fd);
    }
}

void Socks5Server::closeClient(int fd)
{
    close(fd);
    auto conData = workerData.connections[fd];
    workerData.connections.erase(fd);
    if(conData->stat>CON_STAT_GET_IP||(conData->upCon&&workerData.upcons.count(conData->upCon)))
    {
        close(conData->upCon);
        auto upConData = workerData.upcons[conData->upCon];
        workerData.upcons.erase(conData->upCon);
        event_del(&upConData->readEvent);
        event_del(&upConData->writeEvent);
        delete upConData;
    }
    event_del(&conData->readEvent);
    event_del(&conData->writeEvent);
    event_del(&conData->ttlTimer);
    delete conData;
}

UpConn::UpConn(int fd, int clientCon, sockaddr_in &address) :
        fd(fd),clientCon(clientCon),address(address),stat(UPCON_STAT_CONNECTING) {
    event_set(&this->readEvent, fd, EV_READ, Socks5Server::onUpConReadEvent, this);
    event_set(&this->writeEvent, fd, EV_WRITE, Socks5Server::onUpConWriteEvent, this);
}


void Connection::bufferIn(char *data, int dataLen)
{
    this->buffer.reserve(256);
    for(int i = 0;i<dataLen;i++)
    {
        this->buffer.push_back(data[i]);
    }
}

CONNECTION_STAT Socks5Server::connectedStatShift(Connection &con, char *recved, int length) {
    con.bufferIn(recved,length);
    AuthGetRequest request;
    try {
        Protocol::parse((unsigned char *)con.buffer.data(),con.buffer.size(),request);
    } catch (...) {
        return con.stat;
    }
    if(request.version!=0x05)
    {
        return CON_STAT_CLOSE;
    }
    auto authMethod = choseAuthMethod(request.methods);
    AuthGetReply reply;
    reply.version = 0x05;
    reply.authMethod = authMethod;
    unsigned char sndBuffer[50];
    int sndLen = Protocol::build(sndBuffer,sizeof(sndBuffer),reply);
    sendToClient(con.fd,(char*)sndBuffer,sndLen);
    con.buffer.clear();
    con.authMethod = reply.authMethod;
    if(con.authMethod == AUTH_NONE)
    {
        return CON_STAT_CMD;
    }
    else
    {
        return CON_STAT_AUTH;
    }
}

char Socks5Server::choseAuthMethod(std::vector<unsigned char>& supported) {
    for(auto x:supported)
    {
        if(x == AUTH_NONE&&ALLOW_ANONYMOUSE)
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


void Socks5Server::sendToClient(int fd, const char *data, int length)
{
    auto con = workerData.connections[fd];
    con->snd.writeIn((const char *)data,length);
    addEvent(&con->writeEvent,workerData.eventBase);
}

void Socks5Server::sendToUpServer(int fd, const unsigned char *data, int length)
{
    auto con = workerData.upcons[fd];
    con->snd.writeIn((const char *)data,length);
    addEvent(&con->writeEvent,workerData.eventBase);
}

CONNECTION_STAT Socks5Server::authStatShift(Connection &con, char *recved, int length) {
    con.bufferIn(recved,length);
    Auth_Id_Key_Req req;
    try {
        Protocol::parse((unsigned char*)con.buffer.data(),con.buffer.size(),req);
    }
    catch (...)
    {
        return con.stat;
    }
    Auth_Id_Key_Rep rep;
    if(con.authMethod==AUTH_ID_KEY)
    {
        rep.version = 0x05;
        rep.authResult = AUTH_RESULT_OK;
        try {
            if(id_key.at(req.name) != req.passwd)
            {
                throw std::exception();
            }
        } catch (...) {
            rep.authResult = AUTH_RESULT_ERR;
        }
        char sndBuffer[10];
        int sndLen = Protocol::build((unsigned char*)sndBuffer,sizeof(sndBuffer),rep);
        con.buffer.clear();
        sendToClient(con.fd,sndBuffer,sndLen);
        con.buffer.clear();
        return rep.authResult==AUTH_RESULT_OK?CON_STAT_CMD:con.stat;
    }
    else
    {

    }

}

CONNECTION_STAT Socks5Server::cmdStatShift(Connection &con, char *recved, int length) {
    con.bufferIn(recved,length);
    Client_CMD req;
    try {
        Protocol::parse((unsigned char*)con.buffer.data(),con.buffer.size(),req);
    } catch (...) {
        return con.stat;
    }
    con.buffer.clear();
    if(req.version!=0x05)
    {
        return CON_STAT_CLOSE;
    }
    if(req.cmd==SOCKS_CMD_CONNECT)
    {
        if(req.addr_type==ADDRESS_TYPE_IPV4)
        {
            con.dst.sin_family =AF_INET;
            con.dst.sin_port = req.port;
            con.dst.sin_addr.s_addr = req.addr.ipv4addr;
            int res = connectUpServer(con,con.dst);
            if(res<0)
            {
                char sndBuffer[50];
                Rep_CMD rep;
                rep.version = 0x05;
                rep.response = CMD_RESPONSE_NETWORK_ERR;
                rep.addr_type = ADDRESS_TYPE_IPV4;
                rep.rsv = 0x00;
                int sndLen = Protocol::build((unsigned char*)sndBuffer,sizeof(sndBuffer),rep);
                sendToClient(con.fd,sndBuffer,sndLen);
                return CON_STAT_CMD;
            }
            else
                return CON_STAT_CONNECTING;
        }
        else if(req.addr_type==ADDRESS_TYPE_DOMAIN)
        {
            lookup_host(req.addr.domain,new DnsReq(con.fd,req));
            return CON_STAT_GET_IP;
        }else if(req.addr_type==ADDRESS_TYPE_IPV6)
        {
            //not supported now;
            char sndBuffer[50];
            Rep_CMD rep(0x05,CMD_RESPONSE_TGT_NOTSPT,0x00,0x00,{},0x00);
            int sndLen = Protocol::build((unsigned char*)sndBuffer,sizeof(sndBuffer),rep);
            sendToClient(con.fd,sndBuffer,sndLen);
            return CON_STAT_CMD;
        }
    }
    else
    {
        char sndBuffer[50];
        Rep_CMD rep(0x05,CMD_RESPONSE_NOT_SUPPORTED,0x00,0x00,{},0x00);
        int sndLen = Protocol::build((unsigned char*)sndBuffer,sizeof(sndBuffer),rep);
        sendToClient(con.fd,sndBuffer,sndLen);
        return CON_STAT_CMD;
    }
}

int Socks5Server::connectUpServer(Connection&clientCon,sockaddr_in &address)
{
    int ret = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if(ret < 0)
    {
        return ret;
    }
    ServerUtil::setNonblock(ret);
    int conRes = connect(ret,(sockaddr*)&address,sizeof(address));
    if(conRes==0||(conRes<0&&errno==EINPROGRESS))
    {
        auto newUp = new UpConn(ret,clientCon.fd,address);
        newUp->stat = UPCON_STAT_CONNECTING;
        clientCon.upCon = ret;
        addEvent(&newUp->writeEvent,workerData.eventBase);
        workerData.upcons[ret] = newUp;
        return ret;
    }
    else
    {
        //error
        close(ret);
        return -1;
    }
}

void Socks5Server::onClientWriteEvent(int fd, short events, void *args)
{
    auto &con = *((Connection*)args);
    errno = 0;
    int sndLen = con.snd.writeOut(fd,SEND_UNIT_SIZE);
    if(sndLen<0)
    {
        closeClient(fd);
    }
    else if(con.snd.empty())
    {
        if(con.stat>=CON_STAT_COMMUNICATING)
        {
            addEvent(&workerData.upcons[con.upCon]->readEvent, workerData.eventBase);
        }
    }else if(errno == EAGAIN)
    {
        addEvent(&con.writeEvent,workerData.eventBase);
    }

}

void Socks5Server::onUpConReadEvent(int fd, short events, void *args)
{
    auto &upCon = *((UpConn*)args);
    char* buffer = workerData.buffer;
    int readLen = read(fd,buffer,SEND_UNIT_SIZE);
    resetClientTTL(upCon.clientCon,CLIENT_TTL);
    if(readLen<=0&&errno!=EAGAIN)
    {
        //error or disconnected;
        closeClient(upCon.clientCon);
        return;
    }
    else if(errno==EAGAIN)
    {
        addEvent(&upCon.readEvent,workerData.eventBase);
    }else
    {
        sendToClient(upCon.clientCon,buffer,readLen);
        return;

    }

}

void Socks5Server::addEvent(event *event, event_base *eventBase)
{
    event_base_set(eventBase,event);
    event_add(event,NULL);
}

void Socks5Server::onUpConWriteEvent(int fd, short events, void *args)
{
    char *buffer = workerData.buffer;
    auto &upCon = *((UpConn*)args);
    if(upCon.stat==UPCON_STAT_CONNECTING)
    {
        //check if connection is ready;
        int32_t error = 0;
        socklen_t errorLen = sizeof(error);
        if (getsockopt(fd, SOL_SOCKET, SO_ERROR, (char *)&error, &errorLen) < 0)
        {
            error = 1;
        }
        Rep_CMD repCmd;
        repCmd.addr_type=ADDRESS_TYPE_IPV4;
        repCmd.addr.ipv4addr = htonl(INADDR_ANY);
        repCmd.port = htons(12345);
        repCmd.rsv = 0x00;
        repCmd.version = 0x05;
        if(error==0)
        {
            //connected;
            repCmd.response = CMD_RESPONSE_OK ;
            int sndLen = Protocol::build((unsigned char *)buffer,SEND_UNIT_SIZE,repCmd);
            sendToClient(upCon.clientCon,buffer,sndLen);
            addEvent(&upCon.readEvent,workerData.eventBase);
            upCon.stat = UPCON_STAT_CONNECTED;
            workerData.connections[upCon.clientCon]->stat = CON_STAT_COMMUNICATING;
        }
        else
        {
            //error occurred;
            repCmd.response = CMD_RESPONSE_TARGET_INVALID;
            int sndLen = Protocol::build((unsigned char *)buffer,SEND_UNIT_SIZE,repCmd);
            sendToClient(upCon.clientCon,buffer,sndLen);
            workerData.connections[upCon.clientCon]->stat = CON_STAT_CMD;
        }
    }
    else
    {
        //communicate;
        int sndLen = upCon.snd.writeOut(fd,SEND_UNIT_SIZE);
        if(sndLen<0)
        {
            //error occurred;
            closeClient(upCon.clientCon);
            return;
        }
        else if(upCon.snd.empty())
        {
            //no more data to send;
            addEvent(&workerData.connections[upCon.clientCon]->readEvent,workerData.eventBase);
            return;
        }
        else
        {
            //eagain(buffer full);
            addEvent(&upCon.writeEvent,workerData.eventBase);
        }
    }
}

CONNECTION_STAT Socks5Server::communicatingStatShift(Connection &con, char *recved, int length)
{
    sendToUpServer(con.upCon,(unsigned char*)recved,length);
    return con.stat;
}

void Socks5Server::closeUpServer(int fd)
{
    if(!workerData.upcons.count(fd))
        return;
    close(fd);
    auto upCon = workerData.upcons[fd];
    event_del(&upCon->writeEvent);
    event_del(&upCon->readEvent);
    auto con = workerData.connections[upCon->clientCon];
    con->upCon = NULL;
    delete upCon;
    workerData.upcons.erase(fd);
    return;
}

int Socks5Server::lookup_host(const char *host,DnsReq*context)
{
    struct evutil_addrinfo hints;
    struct evdns_getaddrinfo_request *req;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_flags = EVUTIL_AI_CANONNAME;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    req = evdns_getaddrinfo(workerData.evdnsBase, host,NULL,&hints, dns_callback, (void*)context);
    if (req == NULL)
    {
        return -1;
    }
    return 0;
}

void Socks5Server::dns_callback(int errcode, struct addrinfo *addr, void *ptr) {
    DnsReq *context = (DnsReq *) ptr;
    if (!workerData.connections.count(context->clientFd) ||
        workerData.connections[context->clientFd]->stat != CON_STAT_GET_IP)
    {
        if (addr)evutil_freeaddrinfo(addr);
        return;
    }
    auto &con = *workerData.connections[context->clientFd];
    if (errcode)
    {
        char sndBuffer[50];
        Rep_CMD rep(0x05,CMD_RESPONSE_TARGET_INVALID,ADDRESS_TYPE_IPV4,0,{},0x00);
        int sndLen = Protocol::build((unsigned char *) sndBuffer, sizeof(sndBuffer), rep);
        sendToClient(con.fd, sndBuffer, sndLen);
        con.stat =CON_STAT_CMD;
        return;
    }
    else
    {
        struct evutil_addrinfo *ai = addr;
        if (ai->ai_family == AF_INET)
        {
            struct sockaddr_in *sin = (struct sockaddr_in *) ai->ai_addr;
            sin->sin_addr;
            context->req.port;
            int res = connectUpServer(con,con.dst);
            if(res<0)
            {
                char sndBuffer[500];
                Rep_CMD rep(0x05,CMD_RESPONSE_NETWORK_ERR,0,ADDRESS_TYPE_IPV4,{},1234);
                int sndLen = Protocol::build((unsigned char*)sndBuffer,sizeof(sndBuffer),rep);
                sendToClient(con.fd,sndBuffer,sndLen);
                con.stat = CON_STAT_CMD;
            }
            else
            {
                con.stat= CON_STAT_CONNECTING;

            }
        }
        else if (ai->ai_family == AF_INET6)
        {
            sockaddr_in6 addr;
            struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *) ai->ai_addr;
            sin6->sin6_addr;
            return ;
        }
        if (addr)evutil_freeaddrinfo(addr);
    }
}

void Socks5Server::workerLoopExit()
{
    event_base_loopexit(workerData.eventBase,NULL);

}

void Socks5Server::workerExitClean()
{
    event_base_free(workerData.eventBase);
    evdns_base_free(workerData.evdnsBase,NULL);
    for(auto & connection : workerData.connections)
    {
        close(connection.first);
        delete connection.second;
    }
    workerData.connections.clear();
    for(auto & upcon : workerData.upcons)
    {
        close(upcon.first);
        delete upcon.second;
    }
    event_free(workerData.serverSig);
    delete [] workerData.buffer;
    return;
}

void Socks5Server::stop()
{
    std::cout<<"stopping..."<<std::endl;
    for(auto x:this->evfds)
    {
        eventfd_write(x,WORKER_SIG_STOP);
    }
    std::cout<<"quiting "<<this->threadN<<" threads"<<std::endl;
    int i = 0;
    for(auto x:this->threads)
    {
        x->join();
        delete x;
        std::cout<<"quited "<<i++<<std::endl;
    }
    for(auto x:this->evfds)
    {
        close(x);
    }
    this->evfds.clear();
    this->threads.clear();
    std::cout<<"quit correctly!"<<std::endl;
}

void Socks5Server::onNewServerSig(int fd, short events, void *args)
{
    eventfd_t value = 0;
    int res = eventfd_read(fd,&value);
    if(res<0&&errno==EAGAIN)
        return;
    switch (value)
    {
        case WORKER_SIG_STOP:
        {
            workerLoopExit();
        }break;
        case WORKER_SIG_CON_NUM:
        {
            std::cout<<"worker thread : client connections:"<<workerData.connections.size()<<"ready connections:"<<workerData.upcons.size()<<std::endl;
        }break;
        default:
            return;
    }
}

void Socks5Server::resetClientTTL(int fd, int32_t time)
{
    if(!workerData.connections.count(fd))
        return ;
    timeval tm{time,0};
    auto &con = *workerData.connections[fd];
    event_base_set(workerData.eventBase,&con.ttlTimer);
    event_add(&con.ttlTimer,&tm);
}

void Socks5Server::onClientTimerEvent(int fd, short events, void *args)
{
    closeClient(((Connection*)(args))->fd);
    return;
}

void Socks5Server::printConNum()
{
    for(auto x:this->evfds)
    {
        eventfd_write(x,WORKER_SIG_CON_NUM);
        sleep(1);
    }
}

void Socks5Server::workerInit(int evfd,int listenfd)
{
    if(SEND_UNIT_SIZE<MIN_BUFFER_SIZE)
        workerData.buffer = new char[MIN_BUFFER_SIZE];
    else
        workerData.buffer = new char[SEND_UNIT_SIZE + 10];

    workerData.eventBase = event_init();

    workerData.evdnsBase = evdns_base_new(workerData.eventBase,1);

    event_set(&workerData.listenEv,listenfd,EV_READ|EV_PERSIST,onNewCon,NULL);
    addEvent(&workerData.listenEv,workerData.eventBase);

    workerData.serverSig = event_new(workerData.eventBase,evfd,EV_READ|EV_PERSIST,onNewServerSig,NULL);
    event_add(workerData.serverSig,NULL);
}


DnsReq::DnsReq(int clientFd, const Client_CMD &cmd):clientFd(clientFd),req(cmd)
{

}
