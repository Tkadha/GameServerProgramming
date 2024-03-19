#include "stdafx.h"
#include "Client.h"
#include "Board.h"
#include "Input.h"
#include "Player.h"

int main() {
	Input input;
	Client client;
	Player player;

	Board::Init();
	input.Init();

	client.Create_Socket();
	client.Connect_Socket();
	client.Send();
	client.Recv();
	player.Move(client.pos);
	system("cls");

	Board::Render();

	while (true) {
		input.Update();
		if (Input::input) {
			client.Send();
			client.Recv();
			player.Move(client.pos);
			gotoxy(0, 0);
			Board::Render();
		}
		if (Input::command[0] == ESCAPE) break;
	}

	return 0;
}

