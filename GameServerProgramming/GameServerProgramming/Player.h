#pragma once
#include "Board.h"
#include "Input.h"
class Player
{
public:
	Player();
	~Player();
	void Init();
	void Move(Pos);
	void Render();
	void Destroy();
private:
	Pos pos;
};



