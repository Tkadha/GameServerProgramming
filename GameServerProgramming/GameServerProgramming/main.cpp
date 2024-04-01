#include "stdafx.h"
#include "Client.h"
#include "Board.h"
#include "Input.h"
#include "Data.h"
int main() {
	Input input;
	Client client;

	Board::Init();
	input.Init();

	client.Create_Socket();
	client.Connect_Socket();
	// �ʱ� ���� ������ �޾ƿ���
	client.Send();
	while (!login) {
		client.Recv();
		SleepEx(100, TRUE);
	}

	// ���� ���� Ŭ���̾�Ʈ ������ �޾ƿ���
	Input::type = static_cast<char>(E_PACKET_TYPE::E_CONNECT);
	int cnt{ 0 };
	client.Send();
	while (pp.cnt!=-1) {
		client.Recv();
		SleepEx(100, TRUE);
	}

	system("cls");

	gotoxy(Board_X*2+2, 0);
	std::cout << "ESC: ��������";
	gotoxy(Board_X * 2+2, 1);
	std::cout << "����Ű: �̵�";

	Input::type = static_cast<char>(E_PACKET_TYPE::E_ONLINE);
	while (Input::command[0] != ESCAPE) {
		gotoxy(0, 0);
		Board::Update();
		Board::Render();
		input.Update();
		if (Input::input) {
			client.Send();
		}
		client.Recv();
		SleepEx(1, TRUE);
	}
	client.Close_Socket();
	system("cls");
}

