#include "stdafx.h"
#include "Player.h"
Player::Player():pos{0,0}
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
	Board::board[pos.x][pos.y] = E_BOARD_TYPE::E_EMPTY;
	pos = ppos;
	Board::board[pos.x][pos.y] = E_BOARD_TYPE::E_PLAYER;
}

void Player::Render()
{
}

void Player::Destroy()
{
}
