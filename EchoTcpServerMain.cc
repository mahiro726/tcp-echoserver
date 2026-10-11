#include "EchoTcpServer.hpp"
#include <memory>

static void Usage(const std::string &process)
{
    std::cerr << "Usage:\n\t";
    std::cerr << process << " local_port" << std::endl;
}

int main(int argc, char* argv[])
{
    ENABLE_CONSOLE_LOG_STRATEGY();

    if (argc != 2)
    {
        Usage(argv[0]);
        exit(USAGE_ERROR);
    }
    std::uint16_t server_port = std::stoi(argv[1]);

    std::unique_ptr<TcpServer> tsvr = std::make_unique<TcpServer>(server_port);

    tsvr->RegisterService([](const std::string& request )->std::string{
        LOG(LogLevel::DEBUG) << "handler 处理了 " << request;
        return "handler result# " + request;
    });

    tsvr->Init();

    tsvr->Start();
}