#pragma once
#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>
#include <Windows.h>
#include <chrono>

//#pragma comment (lib, "opengl32.lib")
//#pragma comment (lib, "winmm.lib")
//#pragma comment (lib, "ws2_32.lib")

extern int g_left_x;
extern int g_top_y;
extern int g_myid;

extern sf::RenderWindow* g_window;
extern sf::Font g_font;

constexpr int OBJ_NAME_SIZE = 20;

class COBJECT
{
private:
	bool m_showing;
	sf::Sprite m_sprite;
	sf::Text m_name;
	sf::Text m_chat;
	std::chrono::system_clock::time_point m_mess_end_time;
public:
	short sprite_cnt=0;
	short sprite_way=0;
	int m_x, m_y;
	char name[OBJ_NAME_SIZE];
	int		hp;
	int		max_hp;
	int		exp;
	int		level;
	int		visual;
	COBJECT(sf::Texture& t, int x, int y, int x2, int y2) {
		m_showing = false;
		m_sprite.setTexture(t);
		m_sprite.setTextureRect(sf::IntRect(x, y, x2, y2));
		m_mess_end_time = std::chrono::system_clock::now();
	}
	COBJECT(sf::Texture& t, int x, int y, int x2, int y2, int mx, int my) {
		m_showing = false;
		m_sprite.setTexture(t);
		m_sprite.setTextureRect(sf::IntRect(x, y, x2, y2));
		m_mess_end_time = std::chrono::system_clock::now();
		m_x = mx;
		m_y = my;
		sprite_cnt = 0;
		sprite_way = 0;
	}
	COBJECT() {
		m_showing = false;
	}
	void show()
	{
		m_showing = true;
	}
	void hide()
	{
		m_showing = false;
	}
	void sprite_move(int x, int y, int x2, int y2) {
		m_sprite.setTextureRect(sf::IntRect(x, y, x2, y2));
	}
	void a_move(int x, int y) {
		m_sprite.setPosition((float)x, (float)y);
	}

	void a_draw() {
		g_window->draw(m_sprite);
	}

	void move(int x, int y) {
		if (m_x < x)
			sprite_way = 1;
		else if (m_x > x) {
			sprite_way = 0;
		}
		m_x = x;
		m_y = y;
	}
	void draw() {
		if (false == m_showing) return;
		float rx = (m_x - g_left_x) * 65.0f + 1;
		float ry = (m_y - g_top_y) * 65.0f + 1;
		m_sprite.setPosition(rx, ry);
		g_window->draw(m_sprite);
		auto size = m_name.getGlobalBounds();
		if (m_mess_end_time < std::chrono::system_clock::now()) {
			m_name.setPosition(rx + 32 - size.width / 2, ry - 35);
			g_window->draw(m_name);
		}
		else {
			m_chat.setPosition(rx + 32 - size.width / 2, ry - 35);
			g_window->draw(m_chat);
		}
	}
	void set_name(const char str[]) {
		m_name.setFont(g_font);
		m_name.setString(str);
		m_name.setFillColor(sf::Color(0, 0, 0));
		m_name.setStyle(sf::Text::Bold);
	}
	void set_chat(const char str[]) {
		m_chat.setFont(g_font);
		m_chat.setString(str);
		m_chat.setFillColor(sf::Color(255, 255, 255));
		m_chat.setStyle(sf::Text::Bold);
		m_mess_end_time = std::chrono::system_clock::now() + std::chrono::seconds(3);
	}
};

