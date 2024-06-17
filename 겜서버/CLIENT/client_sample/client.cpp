#include <iostream>
#include <unordered_map>
#include "OBJECT.h"
using namespace std;

//#pragma comment (lib, "opengl32.lib")
//#pragma comment (lib, "winmm.lib")
//#pragma comment (lib, "ws2_32.lib")

#include "..\..\SERVER\SERVER\protocol.h"

sf::TcpSocket s_socket;

constexpr auto SCREEN_WIDTH = 20;
constexpr auto SCREEN_HEIGHT = 20;

constexpr auto TILE_WIDTH = 65;
constexpr auto WINDOW_WIDTH = SCREEN_WIDTH * TILE_WIDTH;   // size of window
constexpr auto WINDOW_HEIGHT = SCREEN_WIDTH * TILE_WIDTH;

extern int g_left_x;
extern int g_top_y;
extern int g_myid;

extern sf::RenderWindow* g_window;
extern sf::Font g_font;

COBJECT avatar;
unordered_map <int, COBJECT> objects;

COBJECT red_tile;
COBJECT yellow_tile;
COBJECT background;

sf::Texture* board;
sf::Texture* backimage;
sf::Texture* block;

sf::Texture* mario;
sf::Texture* squid;
sf::Texture* gumba;
sf::Texture* plant;

vector<COBJECT> booms;
sf::Texture* boom;


void client_initialize()
{
	board = new sf::Texture;
	backimage = new sf::Texture;
	block = new sf::Texture;

	mario = new sf::Texture;
	squid = new sf::Texture;
	gumba = new sf::Texture;
	plant = new sf::Texture;

	boom = new sf::Texture;
	board->loadFromFile("bitmap/tile.png");
	backimage->loadFromFile("bitmap/grass.png");
	block->loadFromFile("bitmap/block.png");

	mario->loadFromFile("bitmap/mario.png");
	squid->loadFromFile("bitmap/squid.png");
	gumba->loadFromFile("bitmap/gumba.png");
	plant->loadFromFile("bitmap/plant.png");

	boom->loadFromFile("bitmap/boom.png");

	if (false == g_font.loadFromFile("cour.ttf")) {
		cout << "Font Loading Error!\n";
		exit(-1);
	}
	red_tile = COBJECT{ *board, 0, 0, TILE_WIDTH, TILE_WIDTH };
	yellow_tile = COBJECT{ *board, 65, 0, TILE_WIDTH, TILE_WIDTH };
	avatar = COBJECT{ *mario, 65 * avatar.sprite_cnt, 65 * avatar.sprite_way, 65, 65 };
	avatar.move(4, 4);
	background = COBJECT{ *backimage,0,0,200,200 };
}

void client_finish()
{
	objects.clear();
	delete board;
	delete backimage;
	delete block;

	delete mario;
	delete squid;
	delete gumba;
	delete plant;

	delete boom;
}

void ProcessPacket(char* ptr)
{
	static bool first_time = true;
	switch (ptr[2])
	{
	case SC_LOGIN_FAIL: {
		SC_LOGIN_FAIL_PACKET* packet = reinterpret_cast<SC_LOGIN_FAIL_PACKET*>(ptr);
		cout << "해당 이름은 DB에 없거나 이미 연결된 ID입니다.\n";
		exit(-1);
	}
		break;
	case SC_LOGIN_INFO:
	{
		SC_LOGIN_INFO_PACKET * packet = reinterpret_cast<SC_LOGIN_INFO_PACKET*>(ptr);
		g_myid = packet->id;
		avatar.m_x = packet->x;
		avatar.m_y = packet->y;
		g_left_x = packet->x - SCREEN_WIDTH / 2;
		g_top_y = packet->y - SCREEN_HEIGHT / 2;
		avatar.exp = packet->exp;
		avatar.level = packet->level;
		avatar.hp = packet->hp;
		avatar.max_hp = packet->max_hp;
		avatar.show();
	}
	break;

	case SC_ADD_OBJECT:
	{
		SC_ADD_OBJECT_PACKET * my_packet = reinterpret_cast<SC_ADD_OBJECT_PACKET*>(ptr);
		int id = my_packet->id;

		if (id == g_myid) {
			avatar.move(my_packet->x, my_packet->y);
			g_left_x = my_packet->x - SCREEN_WIDTH / 2;
			g_top_y = my_packet->y - SCREEN_HEIGHT / 2;
			avatar.show();
		}
		else {
			if (my_packet->visual == MARIO) {
				objects[id] = COBJECT{ *mario, 65 * objects[id].sprite_cnt, 65 * objects[id].sprite_way, 65, 65 };
			}
			else if (my_packet->visual == SQUID) {
				objects[id] = COBJECT{ *squid, 65 * objects[id].sprite_cnt, 0, 65, 65 };
			}
			else if (my_packet->visual == GUMBA) {
				objects[id] = COBJECT{ *gumba, 65 * objects[id].sprite_cnt, 0, 65, 65 };
			}
			else if (my_packet->visual == PLANT) {
				objects[id] = COBJECT{ *plant, 65 * objects[id].sprite_cnt, 0, 65, 65 };
			}
			else if (my_packet->visual == BLOCK) {
				objects[id] = COBJECT{ *block, 0, 0, 65, 65 };
			}
			objects[id].visual = my_packet->visual;
			objects[id].move(my_packet->x, my_packet->y);
			objects[id].set_name(my_packet->name);
			objects[id].show();
		}
		break;
	}
	case SC_MOVE_OBJECT:
	{
		SC_MOVE_OBJECT_PACKET* my_packet = reinterpret_cast<SC_MOVE_OBJECT_PACKET*>(ptr);
		int other_id = my_packet->id;
		if (other_id == g_myid) {
			avatar.move(my_packet->x, my_packet->y);
			g_left_x = my_packet->x - SCREEN_WIDTH / 2;
			g_top_y = my_packet->y - SCREEN_HEIGHT / 2;
		}
		else {
			objects[other_id].move(my_packet->x, my_packet->y);
		}
		break;
	}
	case SC_REMOVE_OBJECT:
	{
		SC_REMOVE_OBJECT_PACKET* my_packet = reinterpret_cast<SC_REMOVE_OBJECT_PACKET*>(ptr);
		int other_id = my_packet->id;
		if (other_id == g_myid) {
			avatar.hide();
		}
		else {
			objects.erase(other_id);
		}

		break;
	}
	case SC_CHAT:
	{
		SC_CHAT_PACKET* my_packet = reinterpret_cast<SC_CHAT_PACKET*>(ptr);
		int other_id = my_packet->id;
		if (other_id == g_myid) {
			avatar.set_chat(my_packet->mess);
		}
		else {
			std::cout << my_packet->name << ": " << my_packet->mess << std::endl;
		}
		break;
	}
	case SC_STAT_CHANGE:
	{
		SC_STAT_CHANGE_PACKET* my_packet = reinterpret_cast<SC_STAT_CHANGE_PACKET*>(ptr);
		avatar.exp = my_packet->exp;
		avatar.level = my_packet->level;
		avatar.hp = my_packet->hp;
		avatar.max_hp = my_packet->max_hp;
		break;
	}
	case SC_ATTACK: {
		SC_ATTACK_PACKET* p = reinterpret_cast<SC_ATTACK_PACKET*>(ptr);
		short x = p->x;
		short y = p->y;
		booms.clear();
		booms.push_back(COBJECT{ *boom, 0, 0, 65, 65,x - 1,y });
		booms.push_back(COBJECT{ *boom, 0, 0, 65, 65,x + 1,y });
		booms.push_back(COBJECT{ *boom, 0, 0, 65, 65,x,y - 1 });
		booms.push_back(COBJECT{ *boom, 0, 0, 65, 65,x,y + 1 });
		for (auto& b : booms) {
			b.set_name("");
			b.show();
		}
	}
		break;
	default:
		printf("Unknown PACKET type [%d]\n", ptr[1]);
	}
}

void process_data(char* net_buf, size_t io_byte)
{
	char* ptr = net_buf;
	static size_t in_packet_size = 0;
	static size_t saved_packet_size = 0;
	static char packet_buffer[BUF_SIZE];

	while (0 != io_byte) {
		if (0 == in_packet_size) in_packet_size = ptr[0];
		if (io_byte + saved_packet_size >= in_packet_size) {
			memcpy(packet_buffer + saved_packet_size, ptr, in_packet_size - saved_packet_size);
			ProcessPacket(packet_buffer);
			ptr += in_packet_size - saved_packet_size;
			io_byte -= in_packet_size - saved_packet_size;
			in_packet_size = 0;
			saved_packet_size = 0;
		}
		else {
			memcpy(packet_buffer + saved_packet_size, ptr, io_byte);
			saved_packet_size += io_byte;
			io_byte = 0;
		}
	}
}

void client_main()
{
	char net_buf[BUF_SIZE];
	size_t	received;
	auto recv_result = s_socket.receive(net_buf, BUF_SIZE, received);
	if (recv_result == sf::Socket::Error)
	{
		wcout << L"Recv 에러!";
		exit(-1);
	}
	if (recv_result == sf::Socket::Disconnected) {
		wcout << L"Disconnected\n";
		exit(-1);
	}
	if (recv_result != sf::Socket::NotReady)
		if (received > 0) process_data(net_buf, received);
	for (int i = 0; i < 7; ++i)
		for (int j = 0; j < 7; ++j) {
			background.a_move(200 * i, 200 * j);
			background.a_draw();
		}
	for (int i = 0; i < SCREEN_WIDTH; ++i)
		for (int j = 0; j < SCREEN_HEIGHT; ++j)
		{
			int tile_x = i + g_left_x;
			int tile_y = j + g_top_y;
			if ((tile_x < 0) || (tile_y < 0)) continue;
			if (0 ==(tile_x /3 + tile_y /3) % 2) {
				red_tile.a_move(TILE_WIDTH * i, TILE_WIDTH * j);
				red_tile.a_draw();
			}
			else
			{
				yellow_tile.a_move(TILE_WIDTH * i, TILE_WIDTH * j);
				yellow_tile.a_draw();
			}
		}
	for (auto& pl : objects) pl.second.draw();
	for (auto& b : booms) b.draw();
	avatar.draw();
	sf::Text text;
	text.setFont(g_font);
	text.setFillColor(sf::Color::Black);
	text.setStyle(sf::Text::Bold);
	char buf[100];
	sprintf_s(buf, "(%d, %d)", avatar.m_x, avatar.m_y);
	text.setString(buf);
	g_window->draw(text);

	sprintf_s(buf, "HP: %d / %d  Level: %d  EXP: %d", avatar.hp, avatar.max_hp, avatar.level, avatar.exp);
	text.setString(buf);
	text.setPosition(10, 30);
	g_window->draw(text);
}

void send_packet(void *packet)
{
	unsigned char *p = reinterpret_cast<unsigned char *>(packet);
	size_t sent = 0;
	s_socket.send(packet, p[0], sent);
}

int main()
{
	wcout.imbue(locale("korean"));
	char addr[20];
	std::cout << "서버 주소를 입력하세요.: ";
	std::cin >> addr;

	//char addr[20]{ "127.0.0.1" };

	sf::Socket::Status status = s_socket.connect(addr, PORT_NUM);
	s_socket.setBlocking(false);

	if (status != sf::Socket::Done) {
		wcout << L"서버와 연결할 수 없습니다.\n";
		exit(-1);
	}

	client_initialize();
	CS_LOGIN_PACKET p;
	p.size = sizeof(p);
	p.type = CS_LOGIN;
	p.tester = 0;
	string player_name;
	std::cout << "플레이어의 이름을 적어주세요: ";
	std::cin >> player_name;

	strcpy_s(p.name, player_name.c_str());

	send_packet(&p);
	avatar.set_name(p.name);

	sf::RenderWindow window(sf::VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT), "Simple MMORPG");
	g_window = &window;


	sf::Clock mario_sprite_timer;
	sf::Clock monster_sprite_timer;
	const sf::Time avatar_sprite_time = sf::seconds(0.125f);
	const sf::Time monster_sprite_time = sf::seconds(0.25f);

	sf::Clock attack_clock;
	sf::Time last_attack_time = sf::Time::Zero;
	const sf::Time attack_delay = sf::seconds(1.f);

	sf::Clock move_clock;
	sf::Time last_move_time = sf::Time::Zero;
	const sf::Time move_delay = sf::seconds(0.5f);

	sf::Clock boom_clock;
	sf::Time boom_time = sf::Time::Zero;
	const sf::Time boom_sprite_time = sf::seconds(0.11f);

	while (window.isOpen())
	{
		if (mario_sprite_timer.getElapsedTime() > avatar_sprite_time)
		{
			mario_sprite_timer.restart();
			if (avatar.sprite_cnt < 2)
				++avatar.sprite_cnt;
			else
				avatar.sprite_cnt = 0;
			avatar.sprite_move(65 * avatar.sprite_cnt, 65 * avatar.sprite_way, 65, 65);

			for (auto& pair : objects) {
				if (pair.second.visual == MARIO) {
					if (pair.second.sprite_cnt < 2)
						++pair.second.sprite_cnt;
					else
						pair.second.sprite_cnt = 0;
					pair.second.sprite_move(65 * pair.second.sprite_cnt, 65 * pair.second.sprite_way, 65, 65);
				}
			}
				
		}
		if (monster_sprite_timer.getElapsedTime() > monster_sprite_time)
		{
			monster_sprite_timer.restart();
			for (auto& pair : objects) {
				if (pair.second.visual == BLOCK) continue;
				if (pair.second.visual == MARIO) continue;
				if (pair.second.sprite_cnt < 1)
					++pair.second.sprite_cnt;
				else
					pair.second.sprite_cnt = 0;
				pair.second.sprite_move(65 * pair.second.sprite_cnt, 0, 65, 65);
			}

		}

		if (!booms.empty()) {
			if (boom_clock.getElapsedTime() > boom_sprite_time) {
				boom_clock.restart();
				for (auto& b : booms) {
					if (b.sprite_cnt < 2)
						++b.sprite_cnt;
					else {
						b.sprite_cnt = 0;
						booms.clear();
					}
					b.sprite_move(65 * b.sprite_cnt, 0, 65, 65);
				}
			}
		}
		else
			boom_clock.restart();

		sf::Event event;
		while (window.pollEvent(event))
		{
			if (event.type == sf::Event::Closed)
				window.close();
			if (event.type == sf::Event::KeyPressed) {
				int direction = -1;
				char type{};
				switch (event.key.code) {
				case sf::Keyboard::Left:
					direction = 2;
					break;
				case sf::Keyboard::Right:
					direction = 3;
					break;
				case sf::Keyboard::Up:
					direction = 0;
					break;
				case sf::Keyboard::Down:
					direction = 1;
					break;
				case sf::Keyboard::T:
					type = CS_CHAT;
					break;
				case sf::Keyboard::A:
					type = CS_ATTACK;
					break;
				case sf::Keyboard::Escape:
					CS_LOGOUT_PACKET p;
					p.size = sizeof(p);
					p.type = CS_LOGOUT;
					send_packet(&p);
					window.close();
					break;
				}
				if (-1 != direction) {
					if (move_clock.getElapsedTime() - last_move_time >= move_delay) {
						last_move_time = move_clock.getElapsedTime();
						CS_MOVE_PACKET p;
						p.size = sizeof(p);
						p.type = CS_MOVE;
						p.direction = direction;
						send_packet(&p);
					}
				}
				else {
					switch (type)
					{
					case CS_CHAT:
						char mess[CHAT_SIZE];
						cin >> mess;
						CS_CHAT_PACKET p;
						p.size = sizeof(p);
						p.type = CS_CHAT;
						strcpy_s(p.mess, (char*)mess);
						send_packet(&p);
						break;
					case CS_ATTACK:
						if (attack_clock.getElapsedTime() - last_attack_time >= attack_delay) {
							last_attack_time = attack_clock.getElapsedTime();
							CS_ATTACK_PACKET p;
							p.size = sizeof(p);
							p.type = CS_ATTACK;
							send_packet(&p);
						}
					default:
						break;
					}
				}

			}
		}
		window.clear();
		client_main();
		window.display();
	}
	client_finish();

	return 0;
}