#pragma once

#include <iostream>
#include <string>
#include <strings.h>

#include <sys/socket.h> // socket、bind
#include <netinet/in.h> // struct sockaddr_in 结构体
#include <arpa/inet.h>  // htons、inet_addr

// 对客户端进行描述
class InetAddr
{
public:
    InetAddr(const struct sockaddr_in& address)
    :_address(address),
     _len(sizeof(_address))
    {
        _ip = inet_ntoa(_address.sin_addr);
        _port = ntohs(_address.sin_port);
    }

    InetAddr(uint16_t port, const std::string& ip = "0.0.0.0")
    :_port(port),
     _ip(ip)
    {
        bzero(&_address, sizeof(_address));                   // 清零，防脏值
        _address.sin_family = AF_INET;                     // 协议家族
        _address.sin_addr.s_addr = inet_addr(_ip.c_str()); // sin_addr是一个结构体，要用它的成员变量s_addr来接收
        _address.sin_port = htons(_port);                  // 端口：主机序->网络序
        _len = sizeof(_address);
    }

    bool operator==(const InetAddr& other) const
    {
        return _address.sin_port == other._address.sin_port && _address.sin_addr.s_addr == other._address.sin_addr.s_addr;
    }

    std::string ToString()
    {
        return "[" + _ip + ":" + std::to_string(_port) + "]"; 
    }

    const struct sockaddr_in* GetAddress() const
    {
        return &_address;
    }

    const socklen_t GetLen() const
    {
        return _len;
    }

    const std::string GetIp() const
    {
        return _ip;
    }

    const uint16_t GetPort() const
    {
        return _port;
    }

    ~InetAddr()
    {}

private:
    struct sockaddr_in _address;
    socklen_t _len;
    std::string _ip;
    uint16_t _port;
};