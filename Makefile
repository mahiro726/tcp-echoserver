.PHONY:all
all:client_tcp server_tcp

server_tcp:EchoTcpServerMain.cc
	g++ -o $@ $^ -std=c++17
client_tcp:EchoTcpClient.cc
	g++ -o $@ $^ -std=c++17 -static

.PHONY:clean
clean:
	rm -f client_tcp server_tcp