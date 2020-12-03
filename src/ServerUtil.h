//
// Created by aptx4 on 9/25/2020.
//

#ifndef ENGINED_SERVERUTIL_H
#define ENGINED_SERVERUTIL_H

#include<unistd.h>
#include<fcntl.h>
namespace ServerUtil
{
    int setNonblock(int fd);
    void BlockSigno(int signo);

}


#endif //ENGINED_SERVERUTIL_H
