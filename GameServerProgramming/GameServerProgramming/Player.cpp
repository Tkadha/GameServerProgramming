#include "stdafx.h"
#include <Windows.h>
#include "Board.h"
#include "Player.h"

void gotoxy(Pos ppos)
{
	COORD pos = { static_cast<short>(ppos.x*2), static_cast<short>(ppos.y) };
	SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), pos);
}

Player::Player() :pos{ 0,0 },id{-1}
{
}
Player::Player(Pos ppos, int _id) : pos{ ppos }, id{ _id }
{
}
Player::~Player()
{
}

void Player::Init()
{
}

void Player::Move(Pos ppos)
{
	pos = ppos;
}


void Player::Render()
{
}

void Player::Destroy()
{
}


Pos Player::Getpos()
{
	return pos;
}

void Player::SetBoard_Player()
{
	Board::board[pos.x][pos.y] = E_BOARD_TYPE::E_PLAYER;
}
