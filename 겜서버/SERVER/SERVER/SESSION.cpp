#include "SESSION.h"
#include "EVENT.h"
#include <unordered_map>
extern HANDLE h_iocp;

array<array<Sector, W_WIDTH / SECTOR_SIZE>, W_HEIGHT / SECTOR_SIZE> sectors;
mutex sec_l[W_WIDTH / SECTOR_SIZE][W_HEIGHT / SECTOR_SIZE];
array<array<char, W_WIDTH>, W_HEIGHT> blocks;


bool can_see(SESSION* a, SESSION* b)
{
	int dist = (a->x - b->x) * (a->x - b->x) +
		(a->y - b->y) * (a->y - b->y);
	return dist <= VIEW_RANGE * VIEW_RANGE;
}
bool is_npc(int a)
{
	return a < MAX_NPC || a > MAX_NPC + MAX_USER;
}

void SESSION::set_sector()
{
	_sector_x = x / SECTOR_SIZE;
	_sector_y = y / SECTOR_SIZE;
	sec_l[_sector_x][_sector_y].lock();
	sectors[_sector_x][_sector_y].objects.insert(this);
	sec_l[_sector_x][_sector_y].unlock();
}

void SESSION::do_recv()
{
	DWORD recv_flag = 0;
	memset(&_recv_over._over, 0, sizeof(_recv_over._over));
	_recv_over._wsabuf.len = BUF_SIZE - _prev_remain;
	_recv_over._wsabuf.buf = _recv_over._send_buf + _prev_remain;
	WSARecv(_socket, &_recv_over._wsabuf, 1, 0, &recv_flag, &_recv_over._over, 0);
}

void SESSION::do_send(void* packet)
{
	if (true == is_npc(_id)) return;
	OVER_EXP* sdata = new OVER_EXP{ reinterpret_cast<char*>(packet) };
	WSASend(_socket, &sdata->_wsabuf, 1, 0, 0, &sdata->_over, 0);
}

void SESSION::send_login_fail_packet(const char* name)
{
	SC_LOGIN_FAIL_PACKET p;
	p.size = sizeof(SC_LOGIN_FAIL_PACKET);
	p.type = SC_LOGIN_FAIL;
	do_send(&p);
}

void SESSION::send_login_info_packet()
{
	SC_LOGIN_INFO_PACKET p;
	p.id = _id;
	p.size = sizeof(SC_LOGIN_INFO_PACKET);
	p.type = SC_LOGIN_INFO;
	p.x = x;
	p.y = y;
	p.exp = exp;
	p.hp = hp;
	p.max_hp = max_hp;
	p.level = level;
	p.visual = visual;
	do_send(&p);
}

void SESSION::do_random_move()
{
	unordered_set<SESSION*> old_viewlist;
	for (int i = _sector_x - 1; i <= _sector_x + 1; ++i)
		for (int j = _sector_y - 1; j <= _sector_y + 1; ++j)
			if (i >= 0 && i < W_WIDTH / SECTOR_SIZE && j >= 0 && j < W_HEIGHT / SECTOR_SIZE) {
				sec_l[i][j].lock();
				for (auto obj : sectors[i][j].objects)
					if (obj->_id != _id && can_see(this, obj))
						old_viewlist.insert(obj);
				sec_l[i][j].unlock();
			}

	int before_x = x;
	int before_y = y;

	switch (rand() % 4)
	{
	case 0: if (y > 0)if (!blocks[x][y - 1]) y--; break;
	case 1: if (y < W_HEIGHT - 1)if (!blocks[x][y + 1]) y++; break;
	case 2: if (x > 0)if (!blocks[x - 1][y]) x--; break;
	case 3: if (x < W_WIDTH - 1) if (!blocks[x + 1][y])x++; break;
	}

	if (before_x / SECTOR_SIZE != x / SECTOR_SIZE || before_y / SECTOR_SIZE != y / SECTOR_SIZE) {
		sec_l[_sector_x][_sector_y].lock();
		sectors[_sector_x][_sector_y].objects.erase(this);
		sec_l[_sector_x][_sector_y].unlock();
		set_sector();
	}

	unordered_set<SESSION*> new_viewlist;
	for (int i = _sector_x - 1; i <= _sector_x + 1; ++i)
		for (int j = _sector_y - 1; j <= _sector_y + 1; ++j)
			if (i >= 0 && i < W_WIDTH / SECTOR_SIZE && j >= 0 && j < W_HEIGHT / SECTOR_SIZE) {
				sec_l[i][j].lock();
				for (auto obj : sectors[i][j].objects)
					if (obj->_id != _id && can_see(this, obj)) {
						new_viewlist.insert(obj);
						if (!is_npc(obj->_id) && obj->visual != BLOCK) {
							OVER_EXP* exover = new OVER_EXP;
							exover->_comp_type = OP_PLAYER_MOVE;
							exover->_ai_target_obj = obj->_id;
							PostQueuedCompletionStatus(h_iocp, 1, _id, &exover->_over);
						}
					}
				sec_l[i][j].unlock();
			}

	for (auto pl_obj : new_viewlist) {
		if (0 == old_viewlist.count(pl_obj))
			pl_obj->send_add_object_packet(*this);
		else {
			pl_obj->send_move_packet(*this);
		}
	}
	for (auto pl_obj : old_viewlist)
		if (0 == new_viewlist.count(pl_obj))
			pl_obj->send_remove_object_packet(*this);
}

void SESSION::do_way_move(SESSION& obj)
{
	unordered_set<SESSION*> old_viewlist;
	for (int i = _sector_x - 1; i <= _sector_x + 1; ++i)
		for (int j = _sector_y - 1; j <= _sector_y + 1; ++j)
			if (i >= 0 && i < W_WIDTH / SECTOR_SIZE && j >= 0 && j < W_HEIGHT / SECTOR_SIZE) {
				sec_l[i][j].lock();
				for (auto obj : sectors[i][j].objects)
					if (obj->_id != _id && can_see(this, obj))
						old_viewlist.insert(obj);
				sec_l[i][j].unlock();
			}

	short before_x = x;
	short before_y = y;

	short goal_x = obj.x;
	short goal_y = obj.y;

	switch (rand() % 4)
	{
	case 0: if (y > 0)if (!blocks[x][y - 1]) y--; break;
	case 1: if (y < W_HEIGHT - 1)if (!blocks[x][y + 1]) y++; break;
	case 2: if (x > 0)if (!blocks[x - 1][y]) x--; break;
	case 3: if (x < W_WIDTH - 1) if (!blocks[x + 1][y])x++; break;
	}


	if (before_x / SECTOR_SIZE != x / SECTOR_SIZE || before_y / SECTOR_SIZE != y / SECTOR_SIZE) {
		sec_l[_sector_x][_sector_y].lock();
		sectors[_sector_x][_sector_y].objects.erase(this);
		sec_l[_sector_x][_sector_y].unlock();
		set_sector();
	}

	unordered_set<SESSION*> new_viewlist;
	for (int i = _sector_x - 1; i <= _sector_x + 1; ++i)
		for (int j = _sector_y - 1; j <= _sector_y + 1; ++j)
			if (i >= 0 && i < W_WIDTH / SECTOR_SIZE && j >= 0 && j < W_HEIGHT / SECTOR_SIZE) {
				sec_l[i][j].lock();
				for (auto obj : sectors[i][j].objects)
					if (obj->_id != _id && can_see(this, obj))
						new_viewlist.insert(obj);
				sec_l[i][j].unlock();
			}

	for (auto pl_obj : new_viewlist) {
		if (0 == old_viewlist.count(pl_obj))
			pl_obj->send_add_object_packet(*this);
		else
			pl_obj->send_move_packet(*this);
	}
	for (auto pl_obj : old_viewlist)
		if (0 == new_viewlist.count(pl_obj))
			pl_obj->send_remove_object_packet(*this);
}

void SESSION::send_move_packet(SESSION& obj)
{
	if (true == is_npc(_id)) return;
	SC_MOVE_OBJECT_PACKET p;
	p.id = obj._id;
	p.size = sizeof(SC_MOVE_OBJECT_PACKET);
	p.type = SC_MOVE_OBJECT;
	p.x = obj.x;
	p.y = obj.y;
	p.move_time = obj._last_move_time;
	do_send(&p);
}

void SESSION::send_add_object_packet(SESSION& obj)
{
	if (true == is_npc(_id)) return;
	_vl_l.lock();
	view_list.insert(&obj);
	_vl_l.unlock();

	SC_ADD_OBJECT_PACKET add_packet;
	add_packet.id = obj._id;
	strcpy_s(add_packet.name, obj._name);
	add_packet.size = sizeof(add_packet);
	add_packet.type = SC_ADD_OBJECT;
	add_packet.x = obj.x;
	add_packet.y = obj.y;
	add_packet.visual = obj.visual;
	do_send(&add_packet);
}

void SESSION::send_chat_packet(SESSION& obj, const char* mess, const char* name)
{
	SC_CHAT_PACKET packet;
	packet.id = obj._id;
	strcpy_s(packet.name, name);
	packet.size = sizeof(packet);
	packet.type = SC_CHAT;
	strcpy_s(packet.mess, mess);
	do_send(&packet);
}

void SESSION::send_remove_object_packet(SESSION& obj)
{
	if (true == is_npc(_id)) return;
	_vl_l.lock();
	view_list.erase(&obj);
	_vl_l.unlock();
	SC_REMOVE_OBJECT_PACKET p;
	p.id = obj._id;
	p.size = sizeof(p);
	p.type = SC_REMOVE_OBJECT;
	do_send(&p);
}

void SESSION::send_attack_object_packet()
{
	SC_ATTACK_PACKET p;
	p.size = sizeof(p);
	p.x = x;
	p.y = y;
	p.type = SC_ATTACK;
	do_send(&p);
}

void SESSION::send_stat_change_packet()
{
	SC_STAT_CHANGE_PACKET packet;
	packet.size = sizeof(packet);
	packet.exp = exp;
	packet.hp = hp;
	packet.max_hp = max_hp;
	packet.level = level;
	packet.type = SC_STAT_CHANGE;
	do_send(&packet);
}


void SESSION::do_chat(CS_CHAT_PACKET* p)
{
	for (int i = _sector_x - 1; i <= _sector_x + 1; ++i) {
		for (int j = _sector_y - 1; j <= _sector_y + 1; ++j)
			if (i >= 0 && i < W_WIDTH / SECTOR_SIZE && j >= 0 && j < W_HEIGHT / SECTOR_SIZE) {
				sec_l[i][j].lock();
				for (auto obj : sectors[i][j].objects)
					if (obj->_id != _id && can_see(this, obj)) {
						if (!is_npc(obj->_id)) {
							obj->send_chat_packet(*this, p->mess, _name);
						}
					}
				sec_l[i][j].unlock();
			}
	}
}

