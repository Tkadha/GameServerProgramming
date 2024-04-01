#pragma once

const int Board_X = 8;
const int Board_Y = 8;

const int ESCAPE = 27;

const int UP = 72;
const int DOWN = 80;
const int LEFT = 75;
const int RIGHT = 77;

enum class E_PACKET_TYPE {
	E_LOGIN = 0,
	E_CONNECT,
	E_ONLINE
};

struct Pos {
	int x;
	int y;

	Pos& operator=(const Pos& other) {
		if (this != &other) {
			x = other.x;
			y = other.y;
		}
		return *this;
	}
	bool operator==(const Pos& other) const {
		return (x == other.x && y == other.y);

	}
	bool operator!=(const Pos& other) {
		return !(*this == other);
	}
};

#pragma pack (push,1)
class HEAD_Packet
{
public:
	short size;
	char type;
	int key;
	int id;
	int cnt;
};
class KEYDOWN_Packet: public HEAD_Packet {	
public:
	
};
class POS_Packet : public HEAD_Packet {
public:
	Pos pos;
};
#pragma pack (pop)
