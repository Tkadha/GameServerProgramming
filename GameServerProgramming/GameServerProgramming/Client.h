#pragma once
#include <WS2tcpip.h>



#pragma pack (push,1)
struct keydown_packet {
	short size;
	char type;
	int key;
};
struct pos_packet {
	short size;
	char type;
	Pos pos;
};
#pragma pack (pop)

class Client
{
public:
	Client();
	~Client();
	void Create_Socket();
	void Connect_Socket();
	void Close_Socket();
	void Send();
	void Recv();

	static WSADATA WSAData;
	static SOCKET s_socket;
	static SOCKADDR_IN server_addr;
	Pos pos;
private:

};

