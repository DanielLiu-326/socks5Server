//
// Created by aptx4 on 10/10/2020.
//

#include"ServerConfigs.h"
#include"config_parser/ConfigParser.h"
#include<iostream>
#include<errno.h>
#include<string.h>
#define CHECK_SET_CONFIG_INT(parser,config) \
    if(parser.HasKey("",#config)) \
     config = std::stol(parser.GetConfig("",#config));
#define CHECK_SET_CONFIG_STRING(parser,config) \
    if(parser.HasKey("",#config)) \
     config = parser.GetConfig("",#config);

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

int                                 CLIENT_TTL = 600;
int                                 SEND_UNIT_SIZE = 1500;
std::string                         SERVER_IP = "0.0.0.0";
int                                 PORT = 10091;
bool                                ALLOW_ANONYMOUSE = false;
std::map<std::string,std::string>   ACCOUNTS;
int                                 THREADS = 4;

void initServerConfig(const char * configPath)
{
    CConfigParser configParser;
    if(!configParser.Parser(configPath))
    {
        std::cerr << "cannot open config file" << std::endl;
        throw ServerErr();
    }
    CHECK_SET_CONFIG_INT(configParser,CLIENT_TTL);
    CHECK_SET_CONFIG_INT(configParser,SEND_UNIT_SIZE);
    CHECK_SET_CONFIG_INT(configParser,PORT);
    CHECK_SET_CONFIG_INT(configParser,ALLOW_ANONYMOUSE);
    CHECK_SET_CONFIG_INT(configParser,THREADS);

    CHECK_SET_CONFIG_STRING(configParser,SERVER_IP);
    auto accounts = configParser.GetSectionConfig("accounts");
    for(auto &x : *accounts)
    {
        ACCOUNTS.insert(x);
    }
}