#include "stdafx.h"
#include <Windows.h>
#include "Board.h"
#include "Input.h"

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
	for (int i = 0; i < Board_Y; ++i) {
		for (int j = 0; j < Board_X; ++j) {
			switch (board[j][i])
			{
			case E_BOARD_TYPE::E_EMPTY:
				((i + j) % 2 == 0) ? std::cout << "бр" : std::cout << "бс";
				break;
			case E_BOARD_TYPE::E_PLAYER:
				std::cout << "и▄";
				break;
			default:
				break;
			}
		}
		std::cout << std::endl;
	}
	std::cout << std::endl;
}


void gotoxy(int x, int y)
{
	COORD pos = { static_cast<short>(x), static_cast<short>(y) };
	SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), pos);
}