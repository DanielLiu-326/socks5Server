//
// Created by aptx4 on 10/7/2020.
//

#include "Protocol.h"
#include<string.h>
void Protocol::parse(unsigned char* data,int dataLen , AuthGetRequest & req)
{
    if(dataLen<3)
        throw ParseOverflowException();
    int offset = 2,i = 0;
    req.version = data[0];
    for(;i<data[1]&&i+offset<dataLen;i++)
    {
        req.methods.push_back(data[i+offset]);
    }
    if(offset==dataLen&&i<data[1])
    {
        throw ParseOverflowException();
    }
    return ;
}
void Protocol::parse(unsigned char* data,int dataLen , AuthGetReply & rep)
{
    if(dataLen<2)
    {
        throw ParseOverflowException();
    }
    rep.version = data[0];
    rep.authMethod = data[1];
}
void Protocol::parse(unsigned char* data,int dataLen , Auth_Id_Key_Req & req)
{
    char buffer[256];
    int offset = 0;
    if(dataLen<5)
        throw ParseOverflowException();
    req.version = data[0];

    if(dataLen<data[1]+4)
        throw ParseOverflowException();
    memcpy(buffer,data+2,data[1]);
    buffer[data[1]] = 0;
    req.name=buffer;

    offset = data[1]+2;

    if(dataLen<data[offset]+data[1]+3)
        throw ParseOverflowException();
    memcpy(buffer,data+offset+1,data[offset]);
    buffer[data[offset]] = 0;
    req.passwd = buffer;
}
void Protocol::parse(unsigned char* data,int dataLen,Auth_Id_Key_Rep & rep)
{
    if(dataLen<2)
        throw ParseOverflowException();
    rep.version = data[0];
    rep.authResult = data[1];
}
void Protocol::parse(unsigned char* data,int dataLen,Client_CMD & rep)
{
    if(dataLen<7)
        throw ParseOverflowException();

    rep.version   = data[0];
    rep.cmd       = data[1];
    rep.rsv       = data[2];
    rep.addr_type = data[3];

    if(rep.addr_type==ADDRESS_TYPE_IPV4)
    {
        if(dataLen<6+sizeof(in_addr_t))
            throw ParseOverflowException();
        rep.addr.ipv4addr = *((in_addr_t*)(data+4));
        rep.port =*((int16_t*)(data+8));
    }else if(rep.addr_type==ADDRESS_TYPE_IPV6)
    {
        if(dataLen<6+sizeof(uint8_t)*16)
            throw ParseOverflowException();
        memcpy(rep.addr.ipv6addr,data+4,sizeof(uint8_t)*16);
        rep.port =*((int16_t*)(data+20));
    }else if(rep.addr_type==ADDRESS_TYPE_DOMAIN)
    {
        int domainLen = data[4];
        if(dataLen<domainLen+7)
            throw ParseOverflowException();
        memcpy(rep.addr.domain ,data+5,domainLen);
        rep.addr.domain [domainLen] = 0;
    }
}
 void Protocol::parse(unsigned char* data,int dataLen,Rep_CMD &resp)
{
    if(dataLen<7)
        throw ParseOverflowException();

    resp.version   = data[0];
    resp.response  = data[1];
    resp.rsv       = data[2];
    resp.addr_type = data[3];

    if(resp.addr_type==ADDRESS_TYPE_IPV4)
    {
        if(dataLen<6+sizeof(in_addr_t))
            throw ParseOverflowException();
        resp.addr.ipv4addr = *((in_addr_t*)(data+4));
        resp.port =*((int16_t*)(data+8));
    }else if(resp.addr_type==ADDRESS_TYPE_IPV6)
    {
        if(dataLen<6+sizeof(uint8_t)*16)
            throw ParseOverflowException();
        memcpy(resp.addr.ipv6addr,data+4,sizeof(uint8_t)*16);
        resp.port =*((int16_t*)(data+20));
    }else if(resp.addr_type==ADDRESS_TYPE_DOMAIN)
    {
        int domainLen = data[4];
        if(dataLen<domainLen+7)
            throw ParseOverflowException();
        memcpy(resp.addr.domain ,data+5,domainLen);
        resp.addr.domain[domainLen] = 0;
    }
}
int Protocol::build(unsigned char* buffer , int bufferLen , AuthGetRequest & req)
{
    int offset = 0;
    if(bufferLen<req.methods.size()+1)
        throw ParseOverflowException();
    buffer[offset++] = req.version;
    for(auto x:req.methods)
    {
        buffer[offset++] = x;
    }
    return offset;
}
int Protocol::build(unsigned char* buffer , int bufferLen , AuthGetReply & req)
{
    if(bufferLen<2)
        throw ParseOverflowException();
    buffer[0] = req.version;
    buffer[1] = req.authMethod;
    return 2;
}
int Protocol::build(unsigned char* buffer , int bufferLen , Auth_Id_Key_Req & req)
{
    if(bufferLen<3+req.name.length()+req.passwd.length())
        throw ParseOverflowException();
    req.version = buffer[0];
    buffer[1] = req.name.length();
    strcpy((char*)(buffer+2),req.name.c_str());
    buffer[2+req.name.length()] = req.passwd.length();
    strcpy((char*)buffer+3+req.name.length(),req.passwd.c_str());
    return 3+req.name.length()+req.passwd.length();
}
int Protocol::build(unsigned char* buffer , int bufferLen , Auth_Id_Key_Rep & req)
{
    if(bufferLen<2)
        throw ParseOverflowException();
    buffer[0] = req.version;
    buffer[1] = req.authResult;
    return 2;
}
int Protocol::build(unsigned char* buffer , int bufferLen , Client_CMD & req)
{
    if(bufferLen<6)
        ParseOverflowException();
    buffer[0] = req.version;
    buffer[1] = req.cmd;
    buffer[2] = req.rsv;
    buffer[3] = req.addr_type;
    switch (req.addr_type)
    {
        case ADDRESS_TYPE_IPV4:
        {
            if(bufferLen<6+sizeof(in_addr_t))
                throw ParseOverflowException();
            *((in_addr_t*)(buffer+4))   = req.addr.ipv4addr;
            *((int16_t*)(buffer+8))     = req.port;
            return 10;
        }break;
        case ADDRESS_TYPE_DOMAIN:
        {
            int domainLen = strlen(req.addr.domain);
            if(bufferLen<7+domainLen)
                throw ParseOverflowException();
            buffer[4] = domainLen;
            memcpy(buffer+5,req.addr.domain,domainLen);
            *((int16_t*)(buffer+5+domainLen))=req.port;
            return 7+domainLen;
        }break;
        case ADDRESS_TYPE_IPV6:
        {
            if(bufferLen<6+sizeof(uint8_t)*16)
                throw ParseOverflowException();
            memcpy(buffer+4,req.addr.ipv6addr,sizeof(uint8_t)*16);
            *((int16_t*)(buffer+20))=req.port;
            return 22;
        }break;
        default:
            return -1;
    }
}
int Protocol::build(unsigned char* buffer , int bufferLen , Rep_CMD & req)
{
    if(bufferLen<6)
        ParseOverflowException();
    buffer[0] = req.version;
    buffer[1] = req.response;
    buffer[2] = req.rsv;
    buffer[3] = req.addr_type;
    switch (req.addr_type)
    {
        case ADDRESS_TYPE_IPV4:
        {
            if(bufferLen<6+sizeof(in_addr_t))
                throw ParseOverflowException();
            *((in_addr_t*)(buffer+4))   = req.addr.ipv4addr;
            *((int16_t*)(buffer+8))     = req.port;
            return 10;
        }break;
        case ADDRESS_TYPE_DOMAIN:
        {
            int domainLen = strlen(req.addr.domain);
            if(bufferLen<7+domainLen)
                throw ParseOverflowException();
            buffer[4] = domainLen;
            memcpy(buffer+5,req.addr.domain,domainLen);
            *((int16_t*)(buffer+5+domainLen))=req.port;
            return 7+domainLen;
        }break;
        case ADDRESS_TYPE_IPV6:
        {
            if(bufferLen<6+sizeof(uint8_t)*16)
                throw ParseOverflowException();
            memcpy(buffer+4,req.addr.ipv6addr,sizeof(uint8_t)*16);
            *((int16_t*)(buffer+20))=req.port;
            return 22;
        }break;
        default:
            return -1;
    }
}


AuthGetRequest::AuthGetRequest(VERSION version, const std::vector<unsigned char> &methods) :version(version),methods(methods)
{

}


AuthGetReply::AuthGetReply(VERSION version, AUTH_METHOD authMethod) :version(version),authMethod(authMethod)
{

}


Auth_Id_Key_Req::Auth_Id_Key_Req(VERSION version,const std::string name,const std::string passwd):version(version),name(name),passwd(passwd)
{

}
Auth_Id_Key_Rep::Auth_Id_Key_Rep(VERSION version,unsigned char authResult):version(version),authResult(authResult)
{

}

Client_CMD::Client_CMD(VERSION version, unsigned char cmd, unsigned char rsv, ADDRESS_TYPE addr_type,
                       const Address &addr, PORT_NUM port)
: version(version),cmd(cmd),rsv(rsv),addr_type(addr_type),addr(addr),port(port)
{

}

Rep_CMD::Rep_CMD(VERSION version, CMD_RESPONSE response, unsigned char rsv, ADDRESS_TYPE addr_type, const Address &addr,
                 PORT_NUM port)
:version(version),response(response),rsv(rsv),addr_type(addr_type),addr(addr),port(port)
{

}
