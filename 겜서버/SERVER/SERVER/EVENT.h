#pragma once
#include <chrono>
#include <concurrent_priority_queue.h>

enum EVENT_TYPE { EV_RANDOM_MOVE, EV_HEAL, EV_ATTACK, EV_WAY_MOVE, EV_INVISIBLE };
class EVENT {
public:
	int obj_id;
	std::chrono::system_clock::time_point wakeup_time;
	EVENT_TYPE e_type;
	int target_id;
	bool operator<( const EVENT& rhs) const {
		return wakeup_time > rhs.wakeup_time;
	}
};
