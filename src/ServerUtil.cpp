//
// Created by aptx4 on 9/25/2020.
//

#include "ServerUtil.h"
namespace ServerUtil
{
    int setNonblock(int fd)
    {
        int flags = fcntl(fd, F_GETFL, 0);
        return fcntl(fd, F_SETFL, flags|O_NONBLOCK);
    }
}