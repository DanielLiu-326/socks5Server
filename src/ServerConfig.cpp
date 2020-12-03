//
// Created by danny on 12/3/2020.
//

#include "ServerConfig.h"
#include"config_parser/ConfigParser.h"
#define CHECK_SET_CONFIG_INT(parser,config) \
    if(parser.HasKey("",#config)) \
     config = std::stol(parser.GetConfig("",#config));
#define CHECK_SET_CONFIG_STRING(parser,config) \
    if(parser.HasKey("",#config)) \
     config = parser.GetConfig("",#config);
void ServerConfig::readConfig(std::string path)
{
    CConfigParser configParser;
    if(!configParser.Parser(path))
    {
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
        id_key.insert(x);
    }

}