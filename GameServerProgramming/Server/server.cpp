#include <iostream>
#include <WS2tcpip.h>
#include "../Global.h"

#pragma comment (lib, "WS2_32.LIB")
const short SERVER_PORT = 4000;

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

int main()
{
	short down_program{ 0 };

	WSADATA WSAData;
	WSAStartup(MAKEWORD(2, 0), &WSAData);
	SOCKET s_socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, 0);
	SOCKADDR_IN server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(SERVER_PORT);
	server_addr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	bind(s_socket, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr));
	listen(s_socket, SOMAXCONN);
	INT addr_size = sizeof(server_addr);
	SOCKET c_socket = WSAAccept(s_socket, reinterpret_cast<sockaddr*>(&server_addr), &addr_size, nullptr, 0);
	std::cout << "Client connect " << std::endl;

	// player 시작 위치
	Pos player_pos{ 1,1 };


	while(true){
		KEYDOWN_Packet recevive_p;
		WSABUF mybuf[1];
		mybuf[0].buf = reinterpret_cast<char*>(&recevive_p); mybuf[0].len = sizeof(KEYDOWN_Packet);
		DWORD recv_byte;
		DWORD recv_flag = 0;
		int res = WSARecv(c_socket, mybuf, 1, &recv_byte, &recv_flag, 0, 0);	// 키입력 받아옴
		if (0 != res)
		{
			print_error("WSARecv", WSAGetLastError());
		}		
		if (recevive_p.type == static_cast<char>(E_PACKET_TYPE::E_MOVE)) {
			std::cout << "Client Sent command: " << recevive_p.key << std::endl;
			switch (recevive_p.key)
			{
			case UP:
				if (player_pos.y > 0)
					player_pos.y--;
				break;
			case DOWN:
				if (player_pos.y < Board_Y - 1)
					player_pos.y++;
				break;
			case LEFT:
				if (player_pos.x > 0)
					player_pos.x--;
				break;
			case RIGHT:
				if (player_pos.x < Board_X - 1)
					player_pos.x++;
				break;
			default:
				down_program = recevive_p.key;
				break;
			}
		}
		else if (recevive_p.type == E_PACKET_TYPE::E_LOGIN) {	// 시작 업데이트
		}
		if (down_program == ESCAPE)
			break;
		POS_Packet p;
		p.size = sizeof(POS_Packet);
		p.type = recevive_p.type;
		p.x = player_pos.x;
		p.y = player_pos.y;
		mybuf[0].buf = reinterpret_cast<char*>(&p); mybuf[0].len = p.size;
		DWORD sent_byte;
		WSASend(c_socket, mybuf, 1, &sent_byte, 0, nullptr, nullptr);
	}
	closesocket(c_socket);
	closesocket(s_socket);
	WSACleanup();

	return 0;
}