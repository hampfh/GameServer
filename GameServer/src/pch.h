#pragma once
// Standard libraries
#include <vector>
#include <winsock2.h>
#include <regex>
#include <thread>
#include <iostream>
#include <ctime>
#include <string>
#include <mutex>
#include <random>

// Logging library
#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_sinks.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

// SCL - configuration file library
#include "SCL/SCL.hpp"

typedef std::mt19937 default_random_engine;

#pragma comment(lib,"WS2_32")

enum State {
	none = 0,
	awaiting = 1,
	receiving = 2,
	sending = 3,
};

enum Command {
	start = 0,
	kick = 1
};