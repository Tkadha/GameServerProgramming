#include <iostream>
#include <WS2tcpip.h>
#include <unordered_map>
#include "../Global.h"
#include "server.h"

#pragma comment (lib, "WS2_32.LIB")
const short SERVER_PORT = 4000;
const int BUFSIZE = 256;

std::unordered_map<LPWSAOVERLAPPED, int> g_session_map;	// overlapped 와 id를 map으로 관리함. overlapped pointer로 id를 찾고 세션을 찾기 위해서
std::unordered_map<int, Pos> client_pos;
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


class SESSION;
std::unordered_map <int, SESSION> clients;

void CALLBACK send_callback(DWORD err, DWORD send_size, LPWSAOVERLAPPED send_over, DWORD send_flag);
void CALLBACK recv_callback(DWORD err, DWORD recv_size, LPWSAOVERLAPPED recv_over, DWORD recv_flag);

class EXP_OVER {
public:
	WSAOVERLAPPED _wsa_over;
	size_t _s_id;
	WSABUF _wsa_buf;
	char _buf[BUFSIZE];
public:
	EXP_OVER(size_t s_id, char num_bytes, char* mess) : _s_id(s_id)
	{
		ZeroMemory(&_wsa_over, sizeof(_wsa_over));
		_wsa_buf.buf = _buf;
		_wsa_buf.len = num_bytes;
		memcpy(_buf, mess, num_bytes);
	}
	~EXP_OVER() {}
};

class SESSION {
private:
	int _id;
	WSABUF send_wsabuf[1];
	WSABUF recv_wsabuf[1];
	WSAOVERLAPPED over;
	SOCKET _socket;
	KEYDOWN_Packet kp;
	POS_Packet pp;
	// player 시작 위치
public:
	bool isErase{ false };
	char _recv_buf[BUFSIZE];
	SESSION() {
		std::cout << "Unexpected Constructor Call Error!\n";
		exit(-1);
	}
	SESSION(int id, SOCKET s) : _id(id), _socket(s) {
		g_session_map[&over] = id;
		client_pos[id] = { 1,1 };
		recv_wsabuf[0].buf = reinterpret_cast<char*>(&kp);
		recv_wsabuf[0].len = sizeof(KEYDOWN_Packet);
		send_wsabuf[0].buf = reinterpret_cast<char*>(&pp);
		send_wsabuf[0].len = sizeof(POS_Packet);
	}
	~SESSION() { closesocket(_socket); }

	void do_recv() {
		DWORD recv_flag = 0;
		ZeroMemory(&over, sizeof(over));
		WSARecv(_socket, recv_wsabuf, 1, 0, &recv_flag, &over, recv_callback);
	}

	void do_send(int sender_id) {
		pp.key = clients[sender_id].GetKey();
		pp.pos = client_pos[sender_id];
		pp.id = sender_id;
		pp.size = sizeof(POS_Packet);		
		EXP_OVER* ex_over = new EXP_OVER(sender_id, static_cast<char>(send_wsabuf[0].len), send_wsabuf[0].buf);
		WSASend(_socket, &ex_over->_wsa_buf, 1, 0, 0, &ex_over->_wsa_over, send_callback);
	}

	void Keydown_command() {
		int my_id = g_session_map[&over];
		if (kp.type == static_cast<char>(E_PACKET_TYPE::E_ONLINE)) {
			std::cout << g_session_map[&over] << " Client Sent command: " << kp.key << std::endl;
			switch (kp.key)
			{
			case UP:
				if (client_pos[my_id].y > 0)
					client_pos[my_id].y--;
				break;
			case DOWN:
				if (client_pos[my_id].y < Board_Y - 1)
					client_pos[my_id].y++;
				break;
			case LEFT:
				if (client_pos[my_id].x > 0)
					client_pos[my_id].x--;
				break;
			case RIGHT:
				if (client_pos[my_id].x < Board_X - 1)
					client_pos[my_id].x++;
				break;
			case ESCAPE:
				pp.key = ESCAPE;
				isErase = true;
				break;
			default:
				break;
			}
		}
		else if (kp.type == static_cast<char>(E_PACKET_TYPE::E_CONNECT)) {	// 먼저 켜진 클라 데이터 업데이트
			int cnt = kp.cnt;
			while (true) {
				auto it = clients.find(my_id - (cnt + 1));
				if (it != clients.end()) {
					clients[my_id].do_send(my_id - (cnt + 1));
				}
				// 마지막 클라이언트 연결시
				if (my_id - (cnt + 1) <= 0) {
					pp.cnt = -1;
					break;
				}
				++cnt;
			}
		}
	}

	int GetKey() { return kp.key; }
	char GetType() { return kp.type; }
};



void CALLBACK send_callback(DWORD err, DWORD send_size, LPWSAOVERLAPPED send_over, DWORD send_flag)
{
	delete reinterpret_cast<EXP_OVER*>(send_over);
}
void CALLBACK recv_callback(DWORD err, DWORD recv_size, LPWSAOVERLAPPED recv_over, DWORD recv_flag)
{

	int my_id = g_session_map[recv_over];
	if (0 != err) {
		print_error("WSARecv", WSAGetLastError());
	}
	//-----------
	// 여기서 키입력에 따른 처리 시키기
	clients[my_id].Keydown_command();
	//-----------

	for (auto& cl : clients) {
		cl.second.do_send(my_id);
	}

	if (clients[my_id].isErase) {
		std::cout << "[" << my_id << "] Erase" << std::endl;
		client_pos.erase(my_id);
		clients.erase(my_id);	// id에 해당하는 세션 삭제
		return;
	}
	else
		clients[my_id].do_recv();
}

int main()
{
	std::wcout.imbue(std::locale("korean"));

	WSADATA WSAData;
	WSAStartup(MAKEWORD(2, 2), &WSAData);
	SOCKET s_socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);

	SOCKADDR_IN server_addr;
	ZeroMemory(&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(SERVER_PORT);
	server_addr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);

	bind(s_socket, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr));

	listen(s_socket, SOMAXCONN);

	int addr_size = sizeof(server_addr);
	int id{ 0 };
	while (1) {
		SOCKET c_socket = WSAAccept(s_socket, reinterpret_cast<sockaddr*>(&server_addr), &addr_size, nullptr, 0);
		clients.try_emplace(id, id, c_socket); // 성공하면 true		
		clients[id++].do_recv();
	}
	clients.clear();
	closesocket(s_socket);
	WSACleanup();

	return 0;
}