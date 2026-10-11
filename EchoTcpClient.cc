#include <iostream>
#include <string>
#include "InetAddr.hpp"
#include <unistd.h>  // read/write

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h> // 网络三件套

std::string server_ip = "127.0.0.1";
uint16_t server_port = -1;
int sockfd = -1;

void Usage(const std::string &process)
{
    std::cerr << "Usage:\n\t";
    std::cerr << process << " server_ip server_port";
}


int main(int argc, char* argv[])
{
    if (argc != 3) // 输入的合法性检验
    {
        Usage(argv[0]);
        exit(1);
    }
    server_ip = argv[1];      // 从输入中接收服务端ip
    server_port = atoi(argv[2]); // 接收port

    // 创建套接字
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        std::cerr << "客户端创建 socket 失败" << std::endl;
        exit(1);
    }
    std::cerr << "客户端创建 socket 成功" << std::endl;

    // 需要bind吗？需要; 但是OS会自动bind的，所以不需要自己手动显示bind
    // 那应该干什么呢？发起建立连接的工作
    InetAddr serveraddress(server_port, server_ip);
    int n = connect(sockfd, (struct sockaddr*)serveraddress.GetAddress(), serveraddress.GetLen());
    if (n < 0)
    {
        std::cerr << "connect " << serveraddress.ToString() << " failed" << std::endl;
        exit(2);
    }
    std::cerr << "connect " << serveraddress.ToString() << " success!" << std::endl;

    // 通信
    while(true)
    {
        std::cout << "Please Enter# ";
        std::string message;
        std::getline(std::cin, message);
        ssize_t m = write(sockfd, message.c_str(), message.size());
        if (m < 0)
        {
            std::cerr << "write error" << std::endl;
            continue;
        }
        
        char inbuffer[1024];
        ssize_t l = read(sockfd, inbuffer, sizeof(inbuffer)-1);
        if (l > 0)
        {
            inbuffer[l] = 0;
            std::cout << inbuffer << std::endl;
        }
        else if(l == 0)
        {
            std::cout << "read end of file!" << std::endl;
            break;
        }
        else
        {
            std::cout << "read error!" << std::endl;
            break;
        }
    }
}