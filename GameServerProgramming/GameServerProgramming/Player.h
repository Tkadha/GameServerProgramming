#pragma once
#include <Windows.h>
#include "Board.h"
#include "Input.h"
class Player
{
public:
	Player();
	Player(Pos, int);
	~Player();
	void Init();
	void Move(Pos);
	void Render();
	void Destroy();
	Pos Getpos();
	void SetBoard_Player();
	int id;
private:
	Pos pos;
};



