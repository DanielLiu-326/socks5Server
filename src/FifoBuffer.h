//
// Created by aptx4 on 9/23/2020.
//

#ifndef TESTSERVER_FIFOBUFFER_H
#define TESTSERVER_FIFOBUFFER_H

#include<mutex>
#include<queue>
#include<unistd.h>
#include"RawList.h"
#include<string.h>

#ifndef MIN
#define MIN(a,b) (a)>(b)? (b):(a)
#endif
namespace ServerUtil
{
    template <int size>
    class FifoBlock
    {
    public:
        FifoBlock();
        inline int surplus();
        inline char * dataNow();
        char data[size];
        int dataLen;
        int nowLen;
        friend class RawList<FifoBlock<size> >;
    private:
        FifoBlock<size> * next;
        FifoBlock<size> * last;
    };

    template<int blockSize>
    class FifoBuffer
    {
    public:
        FifoBuffer();
        ~FifoBuffer();
        int writeOut(int fd,int maxSize = -1);
        int writeIn(const char *data,int length);
        int writeInFromFile(int fd,int length);
        inline bool empty();
    private:
        RawList<FifoBlock<blockSize> > buffer;
        std::recursive_mutex lock;

    };

    template<int blockSize>
    int FifoBuffer<blockSize>::writeOut(int fd, int maxSize)//return < 0 error return >=0 write finished or eagain
    {
        int now;
        std::lock_guard<decltype(this->lock)> guard(this->lock);
        for(now= 0 ; (maxSize ==-1 || now<maxSize) && this->buffer.size() ;)
        {
            int len = ::write(fd,buffer.front()->dataNow(),maxSize==-1?buffer.front()->surplus():MIN(buffer.front()->surplus(),maxSize-now));
            if(len<0)
            {
                if(errno==EAGAIN)
                {
                    return now;// no more data
                }
                else
                {
                    return len;//error
                }
            }
            else
            {
                if (len == buffer.front()->surplus())
                {
                    now += len;
                    auto x = this->buffer.front();
                    this->buffer.popFront();
                    delete x;
                }
                else
                {
                    now += len;
                    this->buffer.front()->nowLen += len;
                }
            }
        }
        return now;//maxSize
    }

    template<int blockSize>
    int FifoBuffer<blockSize>::writeIn(const char *data, int length)
    {
        int now;
        std::lock_guard<decltype(this->lock)> guard(this->lock);
        for(now = 0;now < length;)
        {
            if(!this->buffer.size()||this->buffer.back()->dataLen == blockSize)
            {
                this->buffer.pushBack(new FifoBlock<blockSize>);
            }
            else
            {
                int cpLen = MIN(length-now,blockSize - this->buffer.back()->dataLen);
                memcpy(this->buffer.back()->data+this->buffer.back()->dataLen,data+now,cpLen);
                now += cpLen;
                this->buffer.back()->dataLen += cpLen;
            }
        }
        return 0;
    }

    template<int blockSize>
    FifoBuffer<blockSize>::FifoBuffer()
    {

    }

    template<int blockSize>
    FifoBuffer<blockSize>::~FifoBuffer()
    {
        std::lock_guard<decltype(this->lock)> guard(this->lock);
        while(this->buffer.size())
        {
            auto temp = this->buffer.front();
            this->buffer.popFront();
            delete temp;
        }
    }

    template<int blockSize>
    inline bool FifoBuffer<blockSize>::empty()
    {
        std::lock_guard<decltype(this->lock)> guard(this->lock);
        return !this->buffer.size();
    }

    template<int blockSize>
    int FifoBuffer<blockSize>::writeInFromFile(int fd, int length)
    {
        std::lock_guard<decltype(this->lock)> guard(this->lock);
        int nowLen = 0;
        for(;nowLen<length||length==-1;)
        {
            if(!this->buffer.size()||this->buffer.back()->dataLen == blockSize)
            {
                this->poolLock.lock();
                this->buffer.pushBack(new FifoBlock<blockSize>);
                this->poolLock.unlock();
            }
            else
            {
                int readLen = (length==-1
                               ?
                               (blockSize - this->buffer.back()->dataLen)
                               :
                               MIN(length-nowLen,blockSize - this->buffer.back()->dataLen));
                readLen = ::read(fd , this->buffer.back()->data+this->buffer.back()->dataLen,readLen);
                if(readLen<0)
                {
                    return - nowLen;
                }else if(readLen==0)
                {
                    return nowLen;
                }
                nowLen+= readLen;
                this->buffer.back()->dataLen += readLen;
            }
        }
        return nowLen;
    }


    template<int size>
    FifoBlock<size>::FifoBlock():dataLen(0),nowLen(0)
    {

    }

    template<int size>
    inline int FifoBlock<size>::surplus()
    {
        return this->dataLen - nowLen;
    }

    template<int size>
    inline char *FifoBlock<size>::dataNow()
    {
        return this->data+this->nowLen;
    }

}
#endif //TESTSERVER_FIFOBUFFER_H
