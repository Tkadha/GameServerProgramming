#pragma once
#include <iostream>
#include <WS2tcpip.h>
#include <MSWSock.h>
#include <array>
#include <thread>
#include <vector>
#include <queue>
#include <unordered_set>
#include <mutex>

#include "protocol.h"

#include "include//lua.hpp"

#pragma comment(lib, "WS2_32.lib")
#pragma comment(lib, "MSWSock.lib")

using namespace std;


constexpr int VIEW_RANGE = 5;		// 실제 클라이언트 시야보다 약간 작게



