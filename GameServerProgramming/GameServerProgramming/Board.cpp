#include "stdafx.h"
#include <Windows.h>
#include <string>
#include "Board.h"
#include "Input.h"
#include "Data.h"
int Board::board[Board_X][Board_Y];

Board::Board()
{
}

Board::~Board()
{
}

void Board::Init()
{
	for (int i = 0; i < Board_Y; ++i) {
		for (int j = 0; j < Board_X; ++j) {
			board[j][i] = E_BOARD_TYPE::E_EMPTY;
		}
	}
}

void Board::Render()
{	
	std::string draw{};
	for (int i = 0; i < Board_Y; ++i) {
		for (int j = 0; j < Board_X; ++j) {
			if (board[j][i] == E_BOARD_TYPE::E_EMPTY)
				((i + j) % 2 == 0) ? draw += "бр" : draw += "бс";
			else if (board[j][i] == E_BOARD_TYPE::E_PLAYER)
				draw += "и▄";
			else
				draw += "и┘";
		}
		draw += '\n';
	}
	std::cout << draw;
}

void Board::Update()
{
	for (int i = 0; i < Board_Y; ++i) {
		for (int j = 0; j < Board_X; ++j) {
			board[j][i] = E_BOARD_TYPE::E_EMPTY;
		}
	}
	for (auto& cl : players) {
		cl.SetBoard_Player();
	}
	Pos ppos = players[0].Getpos();
	board[ppos.x][ppos.y] = E_BOARD_TYPE::E_MYPLAYER;
}


void gotoxy(int x, int y)
{
	COORD pos = { static_cast<short>(x), static_cast<short>(y) };
	SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), pos);
}