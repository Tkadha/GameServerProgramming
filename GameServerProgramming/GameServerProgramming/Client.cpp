#include "stdafx.h"
#include "Client.h"
#include "Input.h"
#pragma comment (lib, "WS2_32.LIB")

WSADATA Client::WSAData = {  };
SOCKET Client::s_socket = {  };
SOCKADDR_IN Client::server_addr = {  };

const short SERVER_PORT = 4000;
const int ADDRSIZE = 20;
void print_error(const char* msg, int err_no)
{
	WCHAR* msg_buf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, err_no,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPTSTR>(&msg_buf), 0, NULL);
	std::cout << msg;
	std::wcout << L" 에러 " << msg_buf << std::endl;
	while (true); // 디버깅 용
	LocalFree(msg_buf);
}

Client::Client() : pos{ 0,0 }
{
}

Client::~Client()
{
}

void Client::Create_Socket()
{
	static char SERVER_address[ADDRSIZE];
	std::cout << "SERVER 주소를 입력하세요: ";
	std::cin.getline(SERVER_address, ADDRSIZE, '\n');
	std::wcout.imbue(std::locale("korean"));
	WSAStartup(MAKEWORD(2, 0), &WSAData);
	s_socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, 0);
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(SERVER_PORT);
	inet_pton(AF_INET, SERVER_address, &server_addr.sin_addr);
}

void Client::Connect_Socket()
{
	connect(s_socket, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr));
}

void Client::Close_Socket()
{
	closesocket(s_socket);
	WSACleanup();
}

void Client::Send()
{
	keydown_packet p;
	p.size = sizeof(keydown_packet);
	p.type = Input::type;
	p.key = Input::command[0];
	DWORD sent_byte;
	WSABUF mybuf[1];
	mybuf[0].buf = reinterpret_cast<char*>(&p); mybuf[0].len = p.size;
	WSASend(s_socket, mybuf, 1, &sent_byte, 0, 0, 0);
}

void Client::Recv()
{
	pos_packet p;
	WSABUF mybuf_r[1];
	mybuf_r[0].buf = reinterpret_cast<char*>(&p); mybuf_r[0].len = sizeof(pos_packet);
	DWORD recv_byte;
	DWORD recv_flag = 0;
	int res = WSARecv(s_socket, mybuf_r, 1, &recv_byte, &recv_flag, 0, 0);
	if (0 != res)
	{
		print_error("WSARecv", WSAGetLastError());
	}
	pos = p.pos;
}
