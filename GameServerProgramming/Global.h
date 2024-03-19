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
	E_MOVE
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
};

#pragma pack (push,1)
class HEAD_Packet
{
public:
	short size;
	char type;
};
class KEYDOWN_Packet: public HEAD_Packet {	
public:
	int key;
};
class POS_Packet : public HEAD_Packet {
public:
	int x, y;
};
#pragma pack (pop)
