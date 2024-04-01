#include "stdafx.h"
#include "Client.h"
#include "Input.h"
#include "Data.h"
#pragma comment (lib, "WS2_32.LIB")

WSADATA Client::WSAData {  };
SOCKET Client::s_socket {  };
SOCKADDR_IN Client::server_addr {  };
WSAOVERLAPPED Client::s_over { };
WSABUF Client::send_wsabuf[1]{ };
WSABUF Client::recv_wsabuf[1]{ };

const short SERVER_PORT = 4000;
const int ADDRSIZE = 20;
const int BUFSIZE = 256;

POS_Packet pp{};
KEYDOWN_Packet kp{};

int id{};
bool login{false};

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
Client::Client()
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
	WSAStartup(MAKEWORD(2, 2), &WSAData);
	s_socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(SERVER_PORT);
	inet_pton(AF_INET, SERVER_address, &server_addr.sin_addr);

	send_wsabuf[0].buf = reinterpret_cast<char*>(&kp);
	send_wsabuf[0].len = sizeof(KEYDOWN_Packet);
	recv_wsabuf[0].buf = reinterpret_cast<char*>(&pp);
	recv_wsabuf[0].len = sizeof(POS_Packet);
}

void Client::Connect_Socket()
{
	WSAConnect(s_socket, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr), 0, 0, 0, 0);
}

void Client::Close_Socket()
{
	closesocket(s_socket);
	WSACleanup();
}

void Client::Send()
{
	kp.size = sizeof(KEYDOWN_Packet);
	kp.type = Input::type;
	kp.id = id;
	kp.key = Input::command[0];
	memset(&s_over, 0, sizeof(s_over));
	int res = WSASend(s_socket, send_wsabuf, 1, nullptr, 0, &s_over, send_callback);
	if (0 != res) {
		int err_no = WSAGetLastError();
		// 에러 겹친 i/o 작업을 진행하고 있습니다. 라고 나오는 게 정상임
		if (WSA_IO_PENDING != err_no)
			print_error("WSASend", WSAGetLastError());
	}
}
 
void Client::Recv()
{	
	ZeroMemory(&s_over, sizeof(s_over));
	DWORD recv_flag = 0;
	int res = WSARecv(s_socket, recv_wsabuf, 1, nullptr, &recv_flag, &s_over, recv_callback);
	if (0 != res) {
		int err_no = WSAGetLastError();
		// 에러 겹친 i/o 작업을 진행하고 있습니다. 라고 나오는 게 정상임
		if (WSA_IO_PENDING != err_no)
			print_error("WSARecv", WSAGetLastError());
	}

}


void recv_callback(DWORD err, DWORD num_bytes, LPWSAOVERLAPPED over, DWORD flags)
{
	if (!login) {
		id = pp.id;
		login = true;
		Player player(pp.pos, pp.id);
		players.emplace_back(player);
		return;
	}

	bool compare{ false };
	if (kp.type == static_cast<char>(E_PACKET_TYPE::E_CONNECT)) {
		for (auto& cl : players) {
			if (cl.id != pp.id) { continue; }
			compare = true;
			return;
		}
		Player player(pp.pos, pp.id);
		players.emplace_back(player);
		return;
	}

	for (auto& cl : players) {
		if (cl.id != pp.id) { continue; }
		compare = true;
		if (pp.key == ESCAPE) {
			int find_id = pp.id;
			auto it = std::find_if(players.begin(), players.end(), [find_id](const Player& player) {
				return player.id == find_id;
				});
			players.erase(it);
			return;
		}
		cl.Move(pp.pos);
		return;
	}
	Player player(pp.pos, pp.id);
	players.emplace_back(player);
}

void send_callback(DWORD err, DWORD num_bytes, LPWSAOVERLAPPED over, DWORD flags)
{	
}
