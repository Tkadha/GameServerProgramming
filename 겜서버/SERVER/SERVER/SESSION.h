#pragma once
#include "OVER_EXP.h"


enum S_STATE { ST_FREE, ST_ALLOC, ST_INGAME };
enum S_TYPE {AGRO, PEACE};
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
	unordered_set<SESSION*> view_list;
	mutex	_vl_l;

	lua_State* _L;
	mutex _ll;
	atomic_bool _way_move;
	short _way;

	int _sector_x;
	int _sector_y;

	int		_prev_remain;
	int		_last_move_time;

	int		hp;
	int		max_hp;
	int		exp;
	int		level;
	int		visual;				// 종족, 성별등을 구분할 때 사용
	int		atk;
	S_TYPE	type;


	bool invisible;
	bool healing;

	
public:
	SESSION()
	{
		_id = -1;
		_socket = 0;
		x = y = 0;
		_name[0] = 0;
		_state = ST_FREE;
		_prev_remain = 0;

		_way = 0;
		exp = 0;
		level = 1;
		max_hp = 10;
		hp = 10;
		visual = 1;
		atk = 0;
		healing = false;
		invisible = false;
	}

	~SESSION() {}

	void set_sector();

	void do_recv();

	void do_send(void* packet);
	void send_login_fail_packet(const char* name);
	void send_login_info_packet();
	// ID 대신 SESSION을 넘겨보자
	void send_move_packet(SESSION& obj);
	void send_add_object_packet(SESSION& obj);
	void send_chat_packet(SESSION& obj, const char* mess, const char* name);
	void send_remove_object_packet(SESSION& obj);
	void send_attack_object_packet();
	void send_stat_change_packet();
	void do_chat(CS_CHAT_PACKET* p);
	void do_random_move();
	void do_way_move(SESSION& obj);
};

class Sector {
public:
	unordered_set<SESSION*> objects; // 섹터 내의 플레이어 목록
};

constexpr int NPC_START = 0;
constexpr int USER_START = MAX_NPC;


bool can_see(SESSION* a, SESSION* b);
bool is_npc(int a);
