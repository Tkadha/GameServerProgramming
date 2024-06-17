#include <cmath>
#include "SESSION.h"
#include "EVENT.h"
#include "DataBase.h"
using namespace std;

CDataBase db;
array<SESSION, MAX_NPC + MAX_USER + MAX_BLOCK> objects;

extern array<array<Sector, W_WIDTH / SECTOR_SIZE>, W_HEIGHT / SECTOR_SIZE> sectors;
extern mutex sec_l[W_WIDTH / SECTOR_SIZE][W_HEIGHT / SECTOR_SIZE];
extern HANDLE h_iocp;

extern array<array<char, W_WIDTH>, W_HEIGHT> blocks;

concurrency::concurrent_priority_queue<EVENT> g_event_queue;

void add_timer(int id, EVENT_TYPE type, int ms, int p_id)
{
	auto wakeup_time = std::chrono::system_clock::now() + std::chrono::milliseconds(ms);
	g_event_queue.push({ id,wakeup_time,type,p_id });
}

SOCKET g_s_socket, g_c_socket;
OVER_EXP g_a_over;


void disconnect(int c_id);
int get_new_client_id()
{
	for (int i = USER_START; i < USER_START + MAX_USER; ++i) {
		lock_guard <mutex> ll{ objects[i]._s_lock };
		if (objects[i]._state == ST_FREE)
			return i;
	}
	return -1;
}


void process_packet(int c_id, char* packet)
{
	switch (packet[2]) {
	case CS_LOGIN: {
		CS_LOGIN_PACKET* p = reinterpret_cast<CS_LOGIN_PACKET*>(packet);
		atomic_bool find = false;
		for (auto& pl : objects)
			if (pl._name == p->name) {
				objects[c_id].send_login_fail_packet(p->name);
				closesocket(objects[c_id]._socket);
				lock_guard<mutex> ll(objects[c_id]._s_lock);
				objects[c_id]._state = ST_FREE;
				find = true;
				break;
			}
		if (!find) {
			if (!db.FindUserData(p->name)) 	// 없으면 생성
				db.CreateUserData(p->name);

			strcpy_s(objects[c_id]._name, p->name);
			Data data = db.GetUserData(p->name);
			objects[c_id].x = data.x;
			objects[c_id].y = data.y;
			if (p->tester == 1) {
				objects[c_id].x = rand() % W_WIDTH;
				objects[c_id].y = rand() % W_HEIGHT;
			}
			objects[c_id].hp = data.hp;
			objects[c_id].max_hp = data.maxhp;
			objects[c_id].level = data.level;
			objects[c_id].exp = data.exp;
			objects[c_id].visual = MARIO;
			objects[c_id].atk = objects[c_id].level;
			objects[c_id].healing = false;
			objects[c_id].invisible = false;
			objects[c_id].set_sector();
			if (objects[c_id].hp < objects[c_id].max_hp) {
				add_timer(c_id, EV_HEAL, 5000, 0);
				objects[c_id].healing = true;
			}
			objects[c_id].send_login_info_packet();
			{
				lock_guard<mutex> ll{ objects[c_id]._s_lock };
				objects[c_id]._state = ST_INGAME;
			}

			for (int i = objects[c_id]._sector_x - 1; i <= objects[c_id]._sector_x + 1; ++i) {
				for (int j = objects[c_id]._sector_y - 1; j <= objects[c_id]._sector_y + 1; ++j) {
					if (i >= 0 && i < W_WIDTH / SECTOR_SIZE && j >= 0 && j < W_HEIGHT / SECTOR_SIZE) {
						sec_l[i][j].lock();
						for (auto obj : sectors[i][j].objects)
							if (obj->_id != c_id && can_see(&objects[c_id], obj)) {
								obj->send_add_object_packet(objects[c_id]);
								objects[c_id].send_add_object_packet(*obj);
								if (is_npc(obj->_id) && obj->visual != BLOCK && !obj->invisible) {
									OVER_EXP* exover = new OVER_EXP;
									exover->_comp_type = OP_PLAYER_MOVE;
									exover->_ai_target_obj = c_id;
									PostQueuedCompletionStatus(h_iocp, 1, obj->_id, &exover->_over);
									if (obj->_active == false && obj->visual != PLANT ) {
										bool f = false;
										if (true == atomic_compare_exchange_strong(&obj->_active, &f, true)) {
											if(obj->type==AGRO)
												add_timer(obj->_id, EV_WAY_MOVE, 1000, objects[c_id]._id);
											else
												add_timer(obj->_id, EV_RANDOM_MOVE, 1000, objects[c_id]._id);

										}
									}
								}
							}
						sec_l[i][j].unlock();
					}
				}
			}
		}
		break;
	}
	case CS_MOVE: {
		CS_MOVE_PACKET* p = reinterpret_cast<CS_MOVE_PACKET*>(packet);
		objects[c_id]._last_move_time = p->move_time;
		short px = objects[c_id].x;
		short py = objects[c_id].y;
		switch (p->direction) {
		case 0: if (py > 0)if (!blocks[px][py - 1]) py--; break;
		case 1: if (py < W_HEIGHT - 1)if (!blocks[px][py + 1]) py++; break;
		case 2: if (px > 0)if (!blocks[px - 1][py]) px--; break;
		case 3: if (px < W_WIDTH - 1) if (!blocks[px + 1][py])px++; break;
		}

		if (objects[c_id].x / SECTOR_SIZE != px / SECTOR_SIZE || objects[c_id].y / SECTOR_SIZE != py / SECTOR_SIZE) {
			sec_l[objects[c_id]._sector_x][objects[c_id]._sector_y].lock();
			sectors[objects[c_id]._sector_x][objects[c_id]._sector_y].objects.erase(&objects[c_id]);
			sec_l[objects[c_id]._sector_x][objects[c_id]._sector_y].unlock();
			objects[c_id].x = px;
			objects[c_id].y = py;
			objects[c_id].set_sector();
		}
		else {
			objects[c_id].x = px;
			objects[c_id].y = py;
		}
		objects[c_id]._vl_l.lock();
		unordered_set<SESSION*> old_viewlist = objects[c_id].view_list;
		objects[c_id]._vl_l.unlock();
		unordered_set<SESSION*> new_viewlist;

		for (int i = objects[c_id]._sector_x - 1; i <= objects[c_id]._sector_x + 1; ++i) {
			for (int j = objects[c_id]._sector_y - 1; j <= objects[c_id]._sector_y + 1; ++j)
				if (i >= 0 && i < W_WIDTH / SECTOR_SIZE && j >= 0 && j < W_HEIGHT / SECTOR_SIZE) {
					sec_l[i][j].lock();
					for (auto obj : sectors[i][j].objects)
						if (obj->_id != objects[c_id]._id && can_see(&objects[c_id], obj) && !obj->invisible) {
							new_viewlist.insert(obj);
							if (is_npc(obj->_id) && obj->visual != BLOCK && !obj->invisible) {
								OVER_EXP* exover = new OVER_EXP;
								exover->_comp_type = OP_PLAYER_MOVE;
								exover->_ai_target_obj = objects[c_id]._id;
								PostQueuedCompletionStatus(h_iocp, 1, obj->_id, &exover->_over);
								if (obj->_active == false && obj->visual != PLANT) {
									bool f = false;
									if (true == atomic_compare_exchange_strong(&obj->_active, &f, true)) {
										if (obj->type == AGRO)
											add_timer(obj->_id, EV_WAY_MOVE, 1000, objects[c_id]._id);
										else
											add_timer(obj->_id, EV_RANDOM_MOVE, 1000, objects[c_id]._id);
									}
								}
							}
						}
					sec_l[i][j].unlock();
				}
		}

		objects[c_id].send_move_packet(objects[c_id]);

		for (auto p_obj : new_viewlist) {
			if (0 == old_viewlist.count(p_obj)) {
				objects[c_id].send_add_object_packet(*p_obj);
				p_obj->send_add_object_packet(objects[c_id]);
			}
			else {
				p_obj->send_move_packet(objects[c_id]);
			}
		}

		for (auto p_obj : old_viewlist) {
			if (0 == new_viewlist.count(p_obj)) {
				objects[c_id].send_remove_object_packet(*p_obj);
				p_obj->send_remove_object_packet(objects[c_id]);
			}
		}
	}
		break;
	case CS_CHAT: {
		CS_CHAT_PACKET* p = reinterpret_cast<CS_CHAT_PACKET*>(packet);
		objects[c_id].do_chat(p);
	}
		break;
	case CS_ATTACK: {
		CS_ATTACK_PACKET* p = reinterpret_cast<CS_ATTACK_PACKET*>(packet);
		unordered_set<SESSION*> new_list = objects[c_id].view_list;
		for (auto& pl : new_list) {
			if (pl->visual == BLOCK) continue;
			if ((pl->x == objects[c_id].x && abs(pl->y - objects[c_id].y) == 1)
				|| (pl->y == objects[c_id].y && abs(pl->x - objects[c_id].x) == 1)) {
				if (pl->hp - objects[c_id].atk > 0) {
					pl->hp -= objects[c_id].atk;
					char mess[CHAT_SIZE];
					sprintf_s(mess, sizeof(mess), "%s에게 %d의 만큼 데미지를 입혔습니다.", pl->_name, objects[c_id].atk);
					objects[c_id].send_chat_packet(*pl, mess, "system");
				}
				else {
					int reward_exp = pl->level * pl->exp * objects[c_id].level * 2;
					if (pl->type == AGRO) reward_exp *= 2;
					objects[c_id].exp += reward_exp;
					while (objects[c_id].exp > pow(2, objects[c_id].level - 1) * 100) {
						objects[c_id].exp -= pow(2, objects[c_id].level - 1) * 100;
						++objects[c_id].level;
						objects[c_id].atk = objects[c_id].level;
						objects[c_id].max_hp = 10 * objects[c_id].level;
						objects[c_id].hp = objects[c_id].max_hp;
						objects[c_id].send_stat_change_packet();
						char mess[CHAT_SIZE];
						sprintf_s(mess, sizeof(mess), "레벨업 했습니다.");
						objects[c_id].send_chat_packet(*pl, mess, "system");
					}

					db.UpdateUserData(objects[c_id]._name, { objects[c_id].x,objects[c_id].y,objects[c_id].hp,
						objects[c_id].max_hp ,objects[c_id].level ,objects[c_id].exp });

					pl->invisible = true;
					pl->hp = pl->max_hp;
					objects[c_id].send_stat_change_packet();
					char mess[CHAT_SIZE];
					sprintf_s(mess, sizeof(mess), "%s을(를) 잡아서 %d의 경험치를 획득했습니다.", pl->_name, reward_exp);
					objects[c_id].send_chat_packet(*pl, mess, "system");
					objects[c_id].send_remove_object_packet(*pl);
					add_timer(pl->_id, EV_INVISIBLE, 30000, -1);
				}
			}
		}
		objects[c_id].send_attack_object_packet();
	}
		break;
	case CS_TELEPORT: {
		short px = objects[c_id].x;
		short py = objects[c_id].y;

		objects[c_id].x = rand() % W_WIDTH;
		objects[c_id].y = rand() % W_HEIGHT;

		if (objects[c_id].x / SECTOR_SIZE != px / SECTOR_SIZE || objects[c_id].y / SECTOR_SIZE != py / SECTOR_SIZE) {
			sec_l[objects[c_id]._sector_x][objects[c_id]._sector_y].lock();
			sectors[objects[c_id]._sector_x][objects[c_id]._sector_y].objects.erase(&objects[c_id]);
			sec_l[objects[c_id]._sector_x][objects[c_id]._sector_y].unlock();
			objects[c_id].set_sector();
		}

		objects[c_id]._vl_l.lock();
		unordered_set<SESSION*> old_viewlist = objects[c_id].view_list;
		objects[c_id]._vl_l.unlock();
		unordered_set<SESSION*> new_viewlist;

		for (int i = objects[c_id]._sector_x - 1; i <= objects[c_id]._sector_x + 1; ++i) {
			for (int j = objects[c_id]._sector_y - 1; j <= objects[c_id]._sector_y + 1; ++j)
				if (i >= 0 && i < W_WIDTH / SECTOR_SIZE && j >= 0 && j < W_HEIGHT / SECTOR_SIZE) {
					sec_l[i][j].lock();
					for (auto obj : sectors[i][j].objects)
						if (obj->_id != objects[c_id]._id && can_see(&objects[c_id], obj) && !obj->invisible) {
							new_viewlist.insert(obj);
							if (is_npc(obj->_id) && obj->visual != BLOCK && !obj->invisible) {
								OVER_EXP* exover = new OVER_EXP;
								exover->_comp_type = OP_PLAYER_MOVE;
								exover->_ai_target_obj = objects[c_id]._id;
								PostQueuedCompletionStatus(h_iocp, 1, obj->_id, &exover->_over);
								if (obj->_active == false && obj->visual != PLANT) {
									bool f = false;
									if (true == atomic_compare_exchange_strong(&obj->_active, &f, true)) {
										if (obj->type == AGRO)
											add_timer(obj->_id, EV_WAY_MOVE, 1000, objects[c_id]._id);
										else
											add_timer(obj->_id, EV_RANDOM_MOVE, 1000, objects[c_id]._id);
									}
								}
							}
						}
					sec_l[i][j].unlock();
				}
		}

		objects[c_id].send_move_packet(objects[c_id]);

		for (auto p_obj : new_viewlist) {
			if (0 == old_viewlist.count(p_obj)) {
				objects[c_id].send_add_object_packet(*p_obj);
				p_obj->send_add_object_packet(objects[c_id]);
			}
			else {
				p_obj->send_move_packet(objects[c_id]);
			}
		}

		for (auto p_obj : old_viewlist) {
			if (0 == new_viewlist.count(p_obj)) {
				objects[c_id].send_remove_object_packet(*p_obj);
				p_obj->send_remove_object_packet(objects[c_id]);
			}
		}
	}
		break;
	case CS_LOGOUT: {
		CS_LOGOUT_PACKET* p = reinterpret_cast<CS_LOGOUT_PACKET*>(packet);
		disconnect(c_id);
	}
		break;
	}
}

void disconnect(int c_id)
{
	unordered_set<SESSION*> new_viewlist;

	for (auto& pl : objects) {
		{
			lock_guard<mutex> ll(pl._s_lock);
			if (pl._state != ST_INGAME) continue;
		}
		if (pl._id < MAX_NPC) continue;
		if (false == can_see(&objects[c_id], &pl)) continue;
		if (pl._id == c_id) continue;
		pl.send_remove_object_packet(objects[c_id]);
	}
	objects[c_id].send_remove_object_packet(objects[c_id]);

	cout << objects[c_id]._name << " logout\n";
	closesocket(objects[c_id]._socket);

	lock_guard<mutex> ll(objects[c_id]._s_lock);
	objects[c_id]._state = ST_FREE;
}

bool player_exist(int npc_id)
{
	for (int i = USER_START; i < MAX_USER + USER_START; ++i)
	{
		if (ST_INGAME != objects[i]._state) continue;
		if (true == can_see(&objects[npc_id], &objects[i])) return true;
	}
	return false;
}

void worker_thread()
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
				CreateIoCompletionPort(reinterpret_cast<HANDLE>(g_c_socket), h_iocp, client_id, 0);
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
		case OP_SEND: {
			delete ex_over;
		}
			break;
		case OP_RANDOM_MOVE: {
			if (true == player_exist(static_cast<int>(key)) && !objects[key].invisible) {
				objects[key].do_random_move();

				OVER_EXP* exover = new OVER_EXP;
				exover->_comp_type = OP_PLAYER_MOVE;
				exover->_ai_target_obj = ex_over->_ai_target_obj;
				PostQueuedCompletionStatus(h_iocp, 1, key, &exover->_over);

				add_timer(static_cast<int>(key), EV_RANDOM_MOVE, 1000, ex_over->_ai_target_obj);
			}
			else {
				objects[key]._active = false;
			}
			delete ex_over;
		}
			break;
		case OP_WAY_MOVE: {
			if (true == player_exist(static_cast<int>(key)) && !objects[key].invisible) {
				if (objects[key].type == AGRO) {	// a* 알고리즘
					objects[key].do_way_move(objects[ex_over->_ai_target_obj]);

					OVER_EXP* exover = new OVER_EXP;
					exover->_comp_type = OP_PLAYER_MOVE;
					exover->_ai_target_obj = ex_over->_ai_target_obj;
					PostQueuedCompletionStatus(h_iocp, 1, key, &exover->_over);

					add_timer(static_cast<int>(key), EV_WAY_MOVE, 1000, ex_over->_ai_target_obj);
				}
			}
			else {
				objects[key]._active = false;
			}
			delete ex_over;
		}
			break;
		case OP_PLAYER_MOVE: {
			objects[key]._ll.lock();
			auto L = objects[key]._L;
			lua_getglobal(L, "event_player_move");
			lua_pushnumber(L, ex_over->_ai_target_obj);
			lua_pcall(L, 1, 0, 0);
			//lua_pop(L, 1);
			objects[key]._ll.unlock();
			delete ex_over;
		}
			break;
		case OP_REGEN: {
			unordered_set<SESSION*> viewlist;
			for (int i = objects[key]._sector_x - 1; i <= objects[key]._sector_x + 1; ++i)
				for (int j = objects[key]._sector_y - 1; j <= objects[key]._sector_y + 1; ++j)
					if (i >= 0 && i < W_WIDTH / SECTOR_SIZE && j >= 0 && j < W_HEIGHT / SECTOR_SIZE) {
						sec_l[i][j].lock();
						for (auto obj : sectors[i][j].objects)
							if (obj->_id != objects[key]._id && can_see(&objects[key], obj))
								viewlist.insert(obj);
						sec_l[i][j].unlock();
					}
			for (auto pl_obj : viewlist) {
				pl_obj->send_add_object_packet(objects[key]);
			}
		}
			break;
		}
	}
}
int API_get_x(lua_State* L)
{
	int user_id =
		(int)lua_tointeger(L, -1);
	lua_pop(L, 2);
	int x = objects[user_id].x;
	lua_pushnumber(L, x);
	return 1;
}

int API_get_y(lua_State* L)
{
	int user_id =
		(int)lua_tointeger(L, -1);
	lua_pop(L, 2);
	int y = objects[user_id].y;
	lua_pushnumber(L, y);
	return 1;
}

int API_ChangeStat(lua_State* L)
{
	int my_id = (int)lua_tointeger(L, -3);
	int user_id = (int)lua_tointeger(L, -2);
	int atk = (int)lua_tointeger(L, -1);
	if (objects[my_id].invisible) {
		lua_pop(L, 4);
		return 0;
	}
	if (objects[user_id].hp - atk > 0) {
		objects[user_id].hp -= atk;
		lua_pop(L, 4);
		db.UpdateUserData(objects[user_id]._name, { objects[user_id].x,objects[user_id].y,objects[user_id].hp,
			objects[user_id].max_hp ,objects[user_id].level ,objects[user_id].exp });
		objects[user_id].send_stat_change_packet();
		char mess[CHAT_SIZE];
		sprintf_s(mess,sizeof(mess), "%s의 공격으로 %d의 데미지를 입었습니다.", objects[my_id]._name, objects[my_id].atk);
		objects[user_id].send_chat_packet(objects[my_id], mess, "system");
		if(!objects[user_id].healing)
			add_timer(user_id, EV_HEAL, 5000, 0);
		objects[user_id].healing = true;
	}
	else {
		sec_l[objects[user_id]._sector_x][objects[user_id]._sector_y].lock();
		sectors[objects[user_id]._sector_x][objects[user_id]._sector_y].objects.erase(&objects[user_id]);
		sec_l[objects[user_id]._sector_x][objects[user_id]._sector_y].unlock();

		objects[user_id].hp = objects[user_id].max_hp;
		objects[user_id].exp /= 2;
		objects[user_id].x = 1;
		objects[user_id].y = 1;
		objects[user_id].healing = false;

		db.UpdateUserData(objects[user_id]._name, { objects[user_id].x,objects[user_id].y,objects[user_id].hp,
			objects[user_id].max_hp ,objects[user_id].level ,objects[user_id].exp });

		lua_pop(L, 4);
		objects[user_id].set_sector();
		objects[user_id]._vl_l.lock();
		unordered_set<SESSION*> old_viewlist = objects[user_id].view_list;
		objects[user_id]._vl_l.unlock();
		unordered_set<SESSION*> new_viewlist;

		for (int i = objects[user_id]._sector_x - 1; i <= objects[user_id]._sector_x + 1; ++i) {
			for (int j = objects[user_id]._sector_y - 1; j <= objects[user_id]._sector_y + 1; ++j)
				if (i >= 0 && i < W_WIDTH / SECTOR_SIZE && j >= 0 && j < W_HEIGHT / SECTOR_SIZE) {
					sec_l[i][j].lock();
					for (auto obj : sectors[i][j].objects)
						if (obj->_id != objects[user_id]._id && can_see(&objects[user_id], obj) && !obj->invisible) {
							new_viewlist.insert(obj);
							if (is_npc(obj->_id) && obj->visual != BLOCK && !obj->invisible) {
								OVER_EXP* exover = new OVER_EXP;
								exover->_comp_type = OP_PLAYER_MOVE;
								exover->_ai_target_obj = objects[user_id]._id;
								PostQueuedCompletionStatus(h_iocp, 1, obj->_id, &exover->_over);
								if (obj->_active == false && obj->visual != PLANT) {
									bool f = false;
									if (true == atomic_compare_exchange_strong(&obj->_active, &f, true)) {
										if(obj->type == AGRO)
											add_timer(obj->_id, EV_WAY_MOVE, 1000, objects[user_id]._id);
										else
											add_timer(obj->_id, EV_RANDOM_MOVE, 1000, objects[user_id]._id);
									}
								}
							}
						}
					sec_l[i][j].unlock();
				}
		}
		char mess[CHAT_SIZE];
		sprintf_s(mess, sizeof(mess), "%s의 공격으로 사망했습니다...", objects[my_id]._name);
		objects[user_id].send_chat_packet(objects[my_id], mess, "system");
		objects[user_id].send_move_packet(objects[user_id]);

		for (auto p_obj : new_viewlist) {
			if (0 == old_viewlist.count(p_obj)) {
				objects[user_id].send_add_object_packet(*p_obj);
				p_obj->send_add_object_packet(objects[user_id]);
			}
			else {
				p_obj->send_move_packet(objects[user_id]);
			}
		}

		for (auto p_obj : old_viewlist) {
			if (0 == new_viewlist.count(p_obj)) {
				objects[user_id].send_remove_object_packet(*p_obj);
				p_obj->send_remove_object_packet(objects[user_id]);
			}
		}
		objects[user_id].send_stat_change_packet();
	}
	return 0;
}
void Initialize_npc()
{
	std::cout << "NPC intialize begin.\n";
	for (int i = 0; i < MAX_NPC; ++i) {
		objects[i].x = rand() % W_WIDTH;
		objects[i].y = rand() % W_HEIGHT;
		while (blocks[objects[i].x][objects[i].y] == 1) {
			objects[i].x = rand() % W_WIDTH;
			objects[i].y = rand() % W_HEIGHT;
		}
		if (i < MAX_NPC / 3) {
			objects[i].visual = SQUID;
			objects[i].hp = 10;
			objects[i].max_hp = 10;
			objects[i].exp = 15;
			objects[i].level = 2;
			objects[i].atk = 1;
			objects[i].type = PEACE;
			sprintf_s(objects[i]._name, "S%d", i);
		}
		else if (i < MAX_NPC * 2 / 3) {
			objects[i].visual = GUMBA;
			objects[i].hp = 10;
			objects[i].max_hp = 10;
			objects[i].exp = 20;
			objects[i].level = 3;
			objects[i].atk = 2;
			objects[i].type = PEACE;
			sprintf_s(objects[i]._name, "G%d", i);
		}
		else {
			objects[i].visual = PLANT;
			objects[i].hp = 5;
			objects[i].max_hp = 5;
			objects[i].exp = 10;
			objects[i].level = 1;
			objects[i].atk = 3;
			objects[i].type = PEACE;
			sprintf_s(objects[i]._name, "P%d", i);
		}

		objects[i]._id = i;
		objects[i].set_sector();
		objects[i]._state = ST_INGAME;
		objects[i]._active = false;
		objects[i]._npc = true;
		objects[i]._rm_time = chrono::system_clock::now();

		objects[i]._way_move = false;
		objects[i]._way = 0;

		objects[i].invisible = false;

		auto L = objects[i]._L = luaL_newstate();
		luaL_openlibs(L);
		luaL_loadfile(L, "npc.lua");
		lua_pcall(L, 0, 0, 0);

		lua_getglobal(L, "set_uid");
		lua_pushnumber(L, i);
		lua_pcall(L, 1, 0, 0);

		lua_getglobal(L, "set_atk");
		lua_pushnumber(L, objects[i].atk);
		lua_pcall(L, 1, 0, 0);
		// lua_pop(L, 1);// eliminate set_uid from stack after call
		lua_register(L, "API_ChangeStat", API_ChangeStat);
		lua_register(L, "API_get_x", API_get_x);
		lua_register(L, "API_get_y", API_get_y);
	}
	std::cout << "NPC initialize end.\n";
}

void Initialize_block()
{
	for (int i = MAX_NPC + MAX_USER; i < MAX_NPC + MAX_USER + MAX_BLOCK; ++i) {
		objects[i].x = rand() % W_WIDTH;
		objects[i].y = rand() % W_HEIGHT;
		while (blocks[objects[i].x][objects[i].y] == 1) {
			objects[i].x = rand() % W_WIDTH;
			objects[i].y = rand() % W_HEIGHT;
		}
		blocks[objects[i].x][objects[i].y] = 1;
		objects[i].visual = BLOCK;

		objects[i]._id = i;
		objects[i].set_sector();
		objects[i]._state = ST_INGAME;
		objects[i]._active = false;
		objects[i]._npc = true;
		objects[i]._rm_time = chrono::system_clock::now();
		sprintf_s(objects[i]._name, "");

		objects[i].invisible = false;
	}
}
void do_timer()
{
	using namespace chrono;
	while (true) {
		if (!g_event_queue.empty()) {
			EVENT ev;
			if (g_event_queue.try_pop(ev)) {
				if (ev.wakeup_time < system_clock::now()) {
					if (ev.e_type == EV_HEAL) {	// 5초마다 체력 리젠
						if (objects[ev.obj_id].hp + objects[ev.obj_id].max_hp / 10 < objects[ev.obj_id].max_hp) {
							objects[ev.obj_id].hp += objects[ev.obj_id].max_hp / 10;
							objects[ev.obj_id].send_stat_change_packet();
							add_timer(ev.obj_id, EV_HEAL, 5000, 0);
						}
						else {
							objects[ev.obj_id].hp = objects[ev.obj_id].max_hp;
							objects[ev.obj_id].send_stat_change_packet();
							objects[ev.obj_id].healing = false;
						}
					}
					else if (ev.e_type == EV_INVISIBLE) {	// 부활
						objects[ev.obj_id].invisible = false;
						OVER_EXP* ov = new OVER_EXP;
						ov->_comp_type = OP_REGEN;
						if (player_exist(ev.obj_id))
							PostQueuedCompletionStatus(h_iocp, 1, ev.obj_id, &ov->_over);
						else
							objects[ev.obj_id]._active = false;
					}
					else if (ev.e_type == EV_RANDOM_MOVE) {	
						OVER_EXP* ov = new OVER_EXP;
						ov->_comp_type = OP_RANDOM_MOVE;
						ov->_ai_target_obj = ev.target_id;
						if (player_exist(ev.obj_id))
							PostQueuedCompletionStatus(h_iocp, 1, ev.obj_id, &ov->_over);
						else
							objects[ev.obj_id]._active = false;
					}
					else if (ev.e_type == EV_WAY_MOVE) {
						OVER_EXP* ov = new OVER_EXP;
						ov->_comp_type = OP_WAY_MOVE;
						ov->_ai_target_obj = ev.target_id;
						if (player_exist(ev.obj_id))
							PostQueuedCompletionStatus(h_iocp, 1, ev.obj_id, &ov->_over);
						else
							objects[ev.obj_id]._active = false;
					}
				}
				else
					g_event_queue.push(ev);
			}
		}
		else
			this_thread::sleep_for(1ms);
	}
}

void db_update()
{
	while (true) {
		for (int i = USER_START; i < USER_START + MAX_USER; ++i) {
			if (objects[i]._state != ST_INGAME) continue;
			db.UpdateUserData(objects[i]._name, { objects[i].x,objects[i].y,objects[i].hp,objects[i].max_hp ,objects[i].level ,objects[i].exp });
		}
		this_thread::sleep_for(std::chrono::seconds(5));
	}
}

int main()
{
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

	cout << "DataBase Conneting begin\n";
	db.InitializeDB();
	cout << "DataBase Conneting end\n";
	Initialize_block();
	Initialize_npc();

	vector <thread> worker_threads;
	int num_threads = std::thread::hardware_concurrency();
	for (int i = 0; i < num_threads; ++i)
		worker_threads.emplace_back(worker_thread);

	thread timer_thread{ do_timer };
	thread db_thread{ db_update };

	for (auto& th : worker_threads)
		th.join();
	timer_thread.join();
	db_thread.join();
	closesocket(g_s_socket);

	db.CloseDB();
	WSACleanup();
}
