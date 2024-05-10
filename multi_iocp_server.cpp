#include <iostream>
#include <array>
#include <WS2tcpip.h>
#include <MSWSock.h>
#include <thread>
#include <vector>
#include <mutex>
#include <unordered_set>
#include <queue>
#include "protocol.h"

#pragma comment(lib, "WS2_32.lib")
#pragma comment(lib, "MSWSock.lib")
using namespace std;

enum COMP_TYPE { OP_ACCEPT, OP_RECV, OP_SEND, OP_RANDOM_MOVE };

constexpr int VIEW_RANGE = 7;		// 실제 클라이언트 시야보다 약간 작게

class OVER_EXP {
public:
	WSAOVERLAPPED _over;
	WSABUF _wsabuf;
	char _send_buf[BUF_SIZE];
	COMP_TYPE _comp_type;
	OVER_EXP()
	{
		_wsabuf.len = BUF_SIZE;
		_wsabuf.buf = _send_buf;
		_comp_type = OP_RECV;
		ZeroMemory(&_over, sizeof(_over));
	}
	OVER_EXP(char* packet)
	{
		_wsabuf.len = packet[0];
		_wsabuf.buf = _send_buf;
		ZeroMemory(&_over, sizeof(_over));
		_comp_type = OP_SEND;
		memcpy(_send_buf, packet, packet[0]);
	}
};


bool is_npc(int a)
{
	return a < MAX_NPC;
}


enum S_STATE { ST_FREE, ST_ALLOC, ST_INGAME };



class SESSION {
	OVER_EXP _recv_over;

public:
	mutex _s_lock;
	S_STATE _state;
	atomic_bool _active;
	atomic_bool _npc;
	int _id;
	SOCKET _socket;
	short	x, y;
	char	_name[NAME_SIZE];
	chrono::system_clock::time_point _rm_time;
	unordered_set<int> view_list;
	mutex	_vl_l;

	int		_prev_remain;
	int		_last_move_time;
public:
	SESSION()
	{
		_id = -1;
		_socket = 0;
		x = y = 0;
		_name[0] = 0;
		_state = ST_FREE;
		_prev_remain = 0;
	}

	~SESSION() {}

	void do_recv()
	{
		DWORD recv_flag = 0;
		memset(&_recv_over._over, 0, sizeof(_recv_over._over));
		_recv_over._wsabuf.len = BUF_SIZE - _prev_remain;
		_recv_over._wsabuf.buf = _recv_over._send_buf + _prev_remain;
		WSARecv(_socket, &_recv_over._wsabuf, 1, 0, &recv_flag,
			&_recv_over._over, 0);
	}

	void do_send(void* packet)
	{
		if (true == is_npc(_id)) {
			cout << "Send to NPC Error";
			return;
		}
		OVER_EXP* sdata = new OVER_EXP{ reinterpret_cast<char*>(packet) };
		WSASend(_socket, &sdata->_wsabuf, 1, 0, 0, &sdata->_over, 0);
	}
	void send_login_info_packet()
	{
		SC_LOGIN_INFO_PACKET p;
		p.id = _id;
		p.size = sizeof(SC_LOGIN_INFO_PACKET);
		p.type = SC_LOGIN_INFO;
		p.x = x;
		p.y = y;
		do_send(&p);
	}
	void send_move_packet(int c_id);
	void send_add_object_packet(int c_id);
	void send_remove_object_packet(int c_id)
	{
		if (true == is_npc(_id)) return;
		_vl_l.lock();
		view_list.erase(c_id);
		_vl_l.unlock();
		SC_REMOVE_PLAYER_PACKET p;
		p.id = c_id;
		p.size = sizeof(p);
		p.type = SC_REMOVE_PLAYER;
		do_send(&p);
	}
	void do_random_move();

	void heart_beat()
	{
		do_random_move();
	}

	
};

enum EVENT_TYPE {EV_RANDOM_MOVE, EV_HEAL, EV_ATTACK};

class EVENT {
public:
	int obj_id;
	chrono::system_clock::time_point wakeup_time;
	EVENT_TYPE e_type;
	int target_id;
};

priority_queue<EVENT> g_event_queue;
mutex eql;

// ADD TIMER가 EVENT만드는 느낌인듯?
// 정해진 시간이 지나면 깨우기

array<SESSION, MAX_NPC + MAX_USER> objects;
constexpr int NPC_START = 0;
constexpr int USER_START = MAX_NPC;
bool can_see(int a, int b)
{
	int dist = (objects[a].x - objects[b].x) * (objects[a].x - objects[b].x) +
		(objects[a].y - objects[b].y) * (objects[a].y - objects[b].y);
	return dist <= VIEW_RANGE * VIEW_RANGE;
	//if (abs(clients[a].x - clients[b].x) > VIEW_RANGE) return false;
	//return (abs(clients[a].y - clients[b].y) <= VIEW_RANGE);
}

SOCKET g_s_socket, g_c_socket;
OVER_EXP g_a_over;



void SESSION::do_random_move()
{
	unordered_set<int> old_viewlist;
	for (int i = MAX_NPC; i < MAX_NPC + MAX_USER; ++i) {
		if (objects[i]._state != ST_INGAME) continue;
		if (true == can_see(i, _id)) old_viewlist.insert(i);
	}

	switch (rand() % 4)
	{
	case 0: if (x > 0) x--; break;
	case 1: if (x < W_WIDTH - 1) x++; break;
	case 2: if (y > 0) y--; break;
	case 3: if (y < W_HEIGHT - 1) y++; break;
	}
	unordered_set<int> new_viewlist;
	for (int i = MAX_NPC; i < MAX_NPC + MAX_USER; ++i) {
		if (objects[i]._state != ST_INGAME) continue;
		if (true == can_see(i, _id)) new_viewlist.insert(i);
	}

	for (auto pl : new_viewlist) {
		if (0 == old_viewlist.count(pl))
			objects[pl].send_add_object_packet(_id);
		else
			objects[pl].send_move_packet(_id);
	}
	for (auto pl : old_viewlist)
		if (0 == new_viewlist.count(pl))
			objects[pl].send_remove_object_packet(_id);


}

void SESSION::send_move_packet(int c_id)
{
	if (true == is_npc(_id)) return;
	SC_MOVE_PLAYER_PACKET p;
	p.id = c_id;
	p.size = sizeof(SC_MOVE_PLAYER_PACKET);
	p.type = SC_MOVE_PLAYER;
	p.x = objects[c_id].x;
	p.y = objects[c_id].y;
	p.move_time = objects[c_id]._last_move_time;
	do_send(&p);
}

void SESSION::send_add_object_packet(int c_id)
{
	if (true == is_npc(_id)) return;
	_vl_l.lock();
	view_list.insert(c_id);
	_vl_l.unlock();

	SC_ADD_PLAYER_PACKET add_packet;
	add_packet.id = c_id;
	strcpy_s(add_packet.name, objects[c_id]._name);
	add_packet.size = sizeof(add_packet);
	add_packet.type = SC_ADD_PLAYER;
	add_packet.x = objects[c_id].x;
	add_packet.y = objects[c_id].y;
	do_send(&add_packet);
}

int get_new_client_id()
{
	for (int i = MAX_NPC; i < MAX_NPC + MAX_USER; ++i) {
		lock_guard <mutex> ll{ objects[i]._s_lock };
		if (objects[i]._state == ST_FREE)
			return i;
	}
	return -1;
}

void process_packet(int c_id, char* packet)
{
	switch (packet[1]) {
	case CS_LOGIN: {
		CS_LOGIN_PACKET* p = reinterpret_cast<CS_LOGIN_PACKET*>(packet);
		strcpy_s(objects[c_id]._name, p->name);
		objects[c_id].x = rand() % W_WIDTH;
		objects[c_id].y = rand() % W_HEIGHT;
		objects[c_id].send_login_info_packet();
		{
			lock_guard<mutex> ll{ objects[c_id]._s_lock };
			objects[c_id]._state = ST_INGAME;
		}

		for (auto& pl : objects) {
			if (pl._state != ST_INGAME) continue;
			if (false == can_see(c_id, pl._id)) continue;
			if (pl._id == c_id) continue;
			pl.send_add_object_packet(c_id);
			objects[c_id].send_add_object_packet(pl._id);
		}
		
		break;
	}
	case CS_MOVE: {
		CS_MOVE_PACKET* p = reinterpret_cast<CS_MOVE_PACKET*>(packet);
		objects[c_id]._last_move_time = p->move_time;
		short x = objects[c_id].x;
		short y = objects[c_id].y;
		switch (p->direction) {
		case 0: if (y > 0) y--; break;
		case 1: if (y < W_HEIGHT - 1) y++; break;
		case 2: if (x > 0) x--; break;
		case 3: if (x < W_WIDTH - 1) x++; break;
		}
		objects[c_id].x = x;
		objects[c_id].y = y;

		objects[c_id]._vl_l.lock();
		unordered_set<int> old_viewlist = objects[c_id].view_list;
		objects[c_id]._vl_l.unlock();
		unordered_set<int> new_viewlist;

		for (auto& pl : objects) {
			if (pl._state != ST_INGAME) continue;
			if (false == can_see(c_id, pl._id)) continue;
			if (pl._id == c_id) continue;
			new_viewlist.insert(pl._id);
			if (is_npc(pl._id) && pl._active == false) {
				if (true == atomic_compare_exchange_strong(&pl._active, false, true))
					add_timer(pl._id, EV_RANDOM_MOVE, 1000);
			}
		}
		objects[c_id].send_move_packet(c_id);

		for (int p_id : new_viewlist) {
			if (0 == old_viewlist.count(p_id)) {
				objects[c_id].send_add_object_packet(p_id);
				objects[p_id].send_add_object_packet(c_id);
			}
			else {
				objects[p_id].send_move_packet(c_id);
			}
		}

		for (int p_id : old_viewlist) {
			if (0 == new_viewlist.count(p_id)) {
				objects[c_id].send_remove_object_packet(p_id);
				objects[p_id].send_remove_object_packet(c_id);
			}
		}
		objects[c_id]._vl_l.lock();
		objects[c_id].view_list = new_viewlist;
		objects[c_id]._vl_l.unlock();
	}
	}
}

void disconnect(int c_id)
{
	unordered_set<int> new_viewlist;

	for (auto& pl : objects) {
		if (pl._state != ST_INGAME) continue;
		if (false == can_see(c_id, pl._id)) continue;
		if (pl._id == c_id) continue;
		new_viewlist.insert(pl._id);
	}
	objects[c_id].send_remove_object_packet(c_id);

	for (int p_id : new_viewlist) {
		objects[c_id].send_remove_object_packet(p_id);
		objects[p_id].send_remove_object_packet(c_id);
	}


	closesocket(objects[c_id]._socket);

	lock_guard<mutex> ll(objects[c_id]._s_lock);
	objects[c_id]._state = ST_FREE;
}

bool player_exist(int npc_id)
{
	for (int i = USER_START; i < MAX_USER + USER_START; ++i)
	{
		if (ST_INGAME != objects[i]._state) continue;
		if (true == can_see(npc_id, i)) return true;
	}
	return false;
}

void worker_thread(HANDLE h_iocp)
{
	while (true) {
		DWORD num_bytes;
		ULONG_PTR key;
		WSAOVERLAPPED* over = nullptr;
		BOOL ret = GetQueuedCompletionStatus(h_iocp, &num_bytes, &key, &over, INFINITE);
		OVER_EXP* ex_over = reinterpret_cast<OVER_EXP*>(over);
		if (FALSE == ret) {
			if (ex_over->_comp_type == OP_ACCEPT) cout << "Accept Error";
			else {
				cout << "GQCS Error on client[" << key << "]\n";
				disconnect(static_cast<int>(key));
				if (ex_over->_comp_type == OP_SEND) delete ex_over;
				continue;
			}
		}

		if ((0 == num_bytes) && ((ex_over->_comp_type == OP_RECV) || (ex_over->_comp_type == OP_SEND))) {
			disconnect(static_cast<int>(key));
			if (ex_over->_comp_type == OP_SEND) delete ex_over;
			continue;
		}

		switch (ex_over->_comp_type) {
		case OP_ACCEPT: {
			int client_id = get_new_client_id();
			if (client_id != -1) {
				{
					lock_guard<mutex> ll(objects[client_id]._s_lock);
					objects[client_id]._state = ST_ALLOC;
				}
				objects[client_id].x = 0;
				objects[client_id].y = 0;
				objects[client_id]._id = client_id;
				objects[client_id]._name[0] = 0;
				objects[client_id]._prev_remain = 0;
				objects[client_id]._socket = g_c_socket;
				CreateIoCompletionPort(reinterpret_cast<HANDLE>(g_c_socket),
					h_iocp, client_id, 0);
				objects[client_id].do_recv();
				g_c_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
			}
			else {
				cout << "Max user exceeded.\n";
			}
			ZeroMemory(&g_a_over._over, sizeof(g_a_over._over));
			int addr_size = sizeof(SOCKADDR_IN);
			AcceptEx(g_s_socket, g_c_socket, g_a_over._send_buf, 0, addr_size + 16, addr_size + 16, 0, &g_a_over._over);
			break;
		}
		case OP_RECV: {
			int remain_data = num_bytes + objects[key]._prev_remain;
			char* p = ex_over->_send_buf;
			while (remain_data > 0) {
				int packet_size = p[0];
				if (packet_size <= remain_data) {
					process_packet(static_cast<int>(key), p);
					p = p + packet_size;
					remain_data = remain_data - packet_size;
				}
				else break;
			}
			objects[key]._prev_remain = remain_data;
			if (remain_data > 0) {
				memcpy(ex_over->_send_buf, p, remain_data);
			}
			objects[key].do_recv();
			break;
		}
		case OP_SEND:
			delete ex_over;
			break;
		case OP_RANDOM_MOVE:
			if (true == player_exist(key)) {
				objects[key].do_random_move();
				add_timer(key, EV_RANDOM_MOVE, 1000);
			}
			else {
				objects[key]._active = false;
			}
			delete ex_over;
			break;
		}
	}
}

void Initialize_npc()
{
	for (int i = 0; i < MAX_NPC; ++i) {
		objects[i].x = rand() % W_WIDTH;
		objects[i].y = rand() % W_HEIGHT;
		objects[i]._id = i;
		objects[i]._state = ST_INGAME;
		objects[i]._active = false;
		objects[i]._npc = true;
		objects[i]._rm_time = chrono::system_clock::now();
		sprintf_s(objects[i]._name, "N%d", i);
	}
}

void do_ai_old()
{
	using namespace chrono;
	while (true) {
		for (int i = 0; i < MAX_NPC; ++i) {
			if (objects[i]._rm_time < system_clock::now() - 1s) {
				if (1 == i) {
					auto move_delay = system_clock::now() - objects[i]._rm_time;
					cout << "NPC(1) move delay: " << duration_cast<milliseconds>(move_delay).count() << "ms\n";
				}
				objects[i].do_random_move();
				objects[i]._rm_time = system_clock::now();
			}
		}
	}
}

// heart beat
void do_ai_hb()
{
	using namespace chrono;
	while (true) {
		auto start_t = system_clock::now();
		for (int i = 0; i < MAX_NPC; ++i) {
			objects[i].heart_beat();
		}
		auto end_t = system_clock::now();
		auto hb_time = end_t - start_t;
		cout << "heart beat time: " << duration_cast<milliseconds>(hb_time).count() << "ms\n";
		if (hb_time < 1s)
			this_thread::sleep_for(1s - hb_time);
	}
}

//worker thread
void do_ai_wk(HANDLE h_iocp)
{
	using namespace chrono;
	while (true) {
		auto start_t = system_clock::now();
		for (int i = 0; i < MAX_NPC; ++i) {
			OVER_EXP* over = new OVER_EXP;
			over->_comp_type = OP_RANDOM_MOVE;
			PostQueuedCompletionStatus(h_iocp, 1, i, &over->_over);
		}
		auto end_t = system_clock::now();
		auto hb_time = end_t - start_t;
		cout << "heart beat time: " << duration_cast<milliseconds>(hb_time).count() << "ms\n";
		if (hb_time < 1s)
			this_thread::sleep_for(1s - hb_time);
	}
}

void do_timer(HANDLE h_iocp)
{
	using namespace chrono;
	while (true) {
		eql.lock();
		EVENT ev = g_event_queue.top();
		eql.unlock();
		if (ev.wakeup_time < system_clock::now()) {
			eql.lock();
			g_event_queue.pop();
			eql.unlock();
			OVER_EXP* ov = new OVER_EXP;
			ov->_comp_type = OP_RANDOM_MOVE;
			PostQueuedCompletionStatus(h_iocp, 1, ev.obj_id, &ov->_over);
		}
	}
}

int main()
{
	HANDLE h_iocp;

	WSADATA WSAData;
	WSAStartup(MAKEWORD(2, 2), &WSAData);
	g_s_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	SOCKADDR_IN server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PORT_NUM);
	server_addr.sin_addr.S_un.S_addr = INADDR_ANY;
	bind(g_s_socket, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr));
	listen(g_s_socket, SOMAXCONN);
	SOCKADDR_IN cl_addr;
	int addr_size = sizeof(cl_addr);
	h_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
	CreateIoCompletionPort(reinterpret_cast<HANDLE>(g_s_socket), h_iocp, 9999, 0);
	g_c_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	g_a_over._comp_type = OP_ACCEPT;
	AcceptEx(g_s_socket, g_c_socket, g_a_over._send_buf, 0, addr_size + 16, addr_size + 16, 0, &g_a_over._over);

	Initialize_npc();
	thread ai_thread{ do_ai_wk,h_iocp };


	vector <thread> worker_threads;
	int num_threads = std::thread::hardware_concurrency();
	for (int i = 0; i < num_threads; ++i)
		worker_threads.emplace_back(worker_thread, h_iocp);
	for (auto& th : worker_threads)
		th.join();
	ai_thread.join();
	closesocket(g_s_socket);
	WSACleanup();
}
