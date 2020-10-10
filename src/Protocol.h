//
// Created by aptx4 on 10/7/2020.
//

#ifndef SOCKS5SERVER_PROTOCOL_H
#define SOCKS5SERVER_PROTOCOL_H
#include<vector>
#include<string>
#include<sys/types.h>
#include<arpa/inet.h>
#define AUTH_NONE           0x00
#define AUTH_GSSAPI         0x01
#define AUTH_ID_KEY         0x02
#define AUTH_IANA           0x03
#define AUTH_RESERVE        0x04
#define AUTH_NOT_SUPPORT    0xFF

#define AUTH_RESULT_OK      0x00
#define AUTH_RESULT_ERR     0x0F

#define SOCKS_CMD_CONNECT   0x01
#define SOCKS_CMD_BIND      0x02
#define SOCKS_CMD_UDPASS    0x03
#define ADDRESS_TYPE_IPV4   0x01
#define ADDRESS_TYPE_DOMAIN 0x03
#define ADDRESS_TYPE_IPV6   0x04

#define CMD_RESPONSE_OK             0x00
#define CMD_RESPONSE_AGENT_ERR      0x01
#define CMD_RESPONSE_NOT_ALLOWED    0x02
#define CMD_RESPONSE_NETWORK_ERR    0x03
#define CMD_RESPONSE_TARGET_INVALID 0x04
#define CMD_RESPONSE_TARGET_REFUSED 0x05
#define CMD_RESPONSE_TTL_TIMEOUT    0x06
#define CMD_RESPONSE_NOT_SUPPORTED  0x07
#define CMD_RESPONSE_TGT_NOTSPT     0x08


typedef unsigned char AUTH_METHOD;
typedef unsigned char ADDRESS_TYPE;
typedef unsigned char VERSION;
typedef      uint16_t PORT_NUM;
typedef unsigned char CMD_RESPONSE;
struct AuthGetRequest
{
    AuthGetRequest() = default;
    AuthGetRequest(VERSION version,const std::vector<unsigned char> & methods);
    VERSION                     version;
    std::vector<unsigned char>  methods;
};
struct AuthGetReply
{
    AuthGetReply() = default;
    AuthGetReply(VERSION version,AUTH_METHOD authMethod);
    VERSION         version;
    AUTH_METHOD     authMethod;
};
struct Auth_Id_Key_Req
{
    Auth_Id_Key_Req() = default;
    Auth_Id_Key_Req(VERSION version,const std::string name,const std::string passwd);
    VERSION         version;
    std::string     name;
    std::string     passwd;
};
struct Auth_Id_Key_Rep
{
    Auth_Id_Key_Rep() = default;
    Auth_Id_Key_Rep(VERSION version,unsigned char authResult);
    VERSION         version;
    unsigned char   authResult;

};
union Address
{
    in_addr_t       ipv4addr;
    char            domain[256];
    uint8_t         ipv6addr[16];
};
struct Client_CMD
{
    Client_CMD() = default;
    Client_CMD(VERSION version,unsigned char cmd,unsigned char rsv,ADDRESS_TYPE addr_type,const Address& addr,PORT_NUM port);
    VERSION         version;
    unsigned char   cmd;
    unsigned char   rsv;
    ADDRESS_TYPE    addr_type;
    Address         addr;
    PORT_NUM        port;
};
struct Rep_CMD
{
    Rep_CMD() = default;
    Rep_CMD(VERSION version,CMD_RESPONSE response,unsigned char rsv,ADDRESS_TYPE addr,const Address&address,PORT_NUM port);
    VERSION         version;
    CMD_RESPONSE    response;
    unsigned char   rsv;
    ADDRESS_TYPE    addr_type;
    Address         addr;
    PORT_NUM        port;
};
struct ParseOverflowException: public std::exception
{
    ParseOverflowException() noexcept
    {

    }
    const char * what() const noexcept
    {
        return "buffer length too short";
    }
};
class Protocol
{
public:
    static void parse(unsigned char* data , int dataLen , AuthGetRequest & req);
    static void parse(unsigned char* data , int dataLen , AuthGetReply & rep);
    static void parse(unsigned char* data , int dataLen , Auth_Id_Key_Req & req);
    static void parse(unsigned char* data , int dataLen , Auth_Id_Key_Rep & rep);
    static void parse(unsigned char* data , int dataLen , Client_CMD & rep);
    static void parse(unsigned char* data , int dataLen , Rep_CMD &resp);

    static int build(unsigned char* buffer , int bufferLen , AuthGetRequest & req);
    static int build(unsigned char* buffer , int bufferLen , AuthGetReply & req);
    static int build(unsigned char* buffer , int bufferLen , Auth_Id_Key_Req & req);
    static int build(unsigned char* buffer , int bufferLen , Auth_Id_Key_Rep & req);
    static int build(unsigned char* buffer , int bufferLen , Client_CMD & req);
    static int build(unsigned char* buffer , int bufferLen , Rep_CMD & response);
};


#endif //SOCKS5SERVER_PROTOCOL_H
