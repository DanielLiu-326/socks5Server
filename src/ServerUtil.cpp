//
// Created by aptx4 on 9/25/2020.
//

#include "ServerUtil.h"
#include<signal.h>
namespace ServerUtil
{
    int setNonblock(int fd)
    {
        int flags = fcntl(fd, F_GETFL, 0);
        return fcntl(fd, F_SETFL, flags|O_NONBLOCK);
    }
    void BlockSigno(int signo)
    {
        sigset_t signal_mask;
        sigemptyset(&signal_mask);
        sigaddset(&signal_mask, signo);
        pthread_sigmask(SIG_BLOCK, &signal_mask, NULL);
    }
}