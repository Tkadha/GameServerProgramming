#pragma once
class Board
{
public:
	Board();
	~Board();

	static void Init();
	static void Render();

	static int board[Board_X][Board_Y];
private:
};

void gotoxy(int, int);


