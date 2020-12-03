//
// Created by danny on 12/3/2020.
//

#include "Session.h"
#include"Socks5Server.h"
Session::Session(int a_id,Worker*worker):b_id(-1),a_id(a_id),worker(worker),stat(ConStat::NEGOTIATE),dnsReq(NULL)
{
    event_set(&aRead,a_id,EV_READ,Worker::onARead,this);
    event_base_set(worker->eventBase,&this->aRead);
    event_set(&aWrite,a_id,EV_WRITE,Worker::onAWrite,this);
    event_base_set(worker->eventBase,&this->aWrite);
    evtimer_set(&timer,Worker::onSessionTimeOut,this);
    event_base_set(worker->eventBase,&this->timer);
    this->protoBuffer.reserve(512);
}

void Session::close()
{
    if(this->a_id>0)
    {
        ::close(a_id);
    }
    if(this->b_id>0)
    {
        ::close(b_id);
    }
    return;
}

void Session::bufferIn(char *data, int len)
{
    int nowLen = this->protoBuffer.size();
    this->protoBuffer.resize(nowLen+len);
    memcpy(this->protoBuffer.data()+nowLen,data,len);
}

void Session::resetTimer(int sec)
{
    timeval tv;
    evutil_timerclear(&tv);
    tv.tv_sec = sec;
    event_add(&this->timer, &tv);
}

void Session::assignB(int b_id)
{
    this->b_id = b_id;
    event_set(&this->bRead,b_id,EV_READ,Worker::onBRead,this);
    event_base_set(worker->eventBase,&this->bRead);
    event_set(&this->bWrite,b_id,EV_WRITE,Worker::onBWrite,this);
    event_base_set(worker->eventBase,&this->bWrite);

}
