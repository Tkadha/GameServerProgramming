#include "stdafx.h"
#include "Input.h"
int Input::input = 0;
char Input::command[] = {};
char Input::type{ static_cast<char>(E_PACKET_TYPE::E_LOGIN) };
Input::Input()
{

}

Input::~Input()
{

}

void Input::Init()
{
	input = 0;
}

void Input::Update()
{
	input = _kbhit();
	if (input) {
		command[0] = _getch();
		if (command[0] == -32) {
			type = static_cast<char>(E_PACKET_TYPE::E_MOVE);
			command[0] = _getch();
		}
		
	}
}

void Input::Render()
{
}

void Input::Destroy()
{
}