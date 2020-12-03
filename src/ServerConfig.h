//
// Created by danny on 12/3/2020.
//

#ifndef SOCKS5_SERVERCONFIG_H
#define SOCKS5_SERVERCONFIG_H
#include<string>
#include<map>

#include<string.h>
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

struct ServerConfig
{
    void readConfig(std::string path);
    int         CLIENT_TTL;
    int         SEND_UNIT_SIZE;
    std::string SERVER_IP;
    int         PORT;
    int         ALLOW_ANONYMOUSE;
    int         THREADS;
    std::map<std::string,std::string> id_key;
};

#endif //SOCKS5_SERVERCONFIG_H
