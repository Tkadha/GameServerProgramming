#pragma once
#include <WS2tcpip.h>

void CALLBACK send_callback(DWORD err, DWORD num_bytes, LPWSAOVERLAPPED over, DWORD flags);
void CALLBACK recv_callback(DWORD err, DWORD num_bytes, LPWSAOVERLAPPED over, DWORD flags);
extern POS_Packet pp;
extern KEYDOWN_Packet kp;
extern int id;
extern bool login;
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
	static WSAOVERLAPPED s_over;
	static WSABUF send_wsabuf[1];
	static WSABUF recv_wsabuf[1];
private:
};

