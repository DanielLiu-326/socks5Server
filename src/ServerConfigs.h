//
// Created by aptx4 on 10/10/2020.
//

#ifndef SOCKS5SERVER_SERVERCONFIGS_H
#define SOCKS5SERVER_SERVERCONFIGS_H
#include<string>
#include<map>
//unchangeable configs

//changeable configs
extern int                                  CLIENT_TTL;
extern int                                  SEND_UNIT_SIZE;
extern std::string                          SERVER_IP;
extern int                                  PORT;
extern bool                                 ALLOW_ANONYMOUSE;
extern std::map<std::string,std::string>    ACCOUNTS;
extern int                                  THREADS;
void initServerConfig(const char * configPath);

#endif //SOCKS5SERVER_SERVERCONFIGS_H
