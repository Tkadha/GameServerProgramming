#include "Block.h"
#include <array>
#include "protocol.h"

std::array<std::array<char, W_WIDTH>, W_HEIGHT> field;

void set_block()
{
	for (auto& row : field) {
		row.fill(0); // ��� �ʵ带 0���� �ʱ�ȭ
	}

	for (int i = 0; i < Block_Count; ++i) {
		field[rand() % W_WIDTH][rand() % W_HEIGHT] = 1;
	}
}


