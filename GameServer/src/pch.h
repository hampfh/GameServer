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
#include <filesystem>

// Guidelines Support Library
#undef max
#include <gsl/gsl>

// Logging library
#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

// SCL - configuration file library
#include "SCL/SCL.hpp"

typedef std::mt19937 default_random_engine;

#pragma comment(lib,"WS2_32")

namespace hgs {
	enum State {
		none = 0,
		receiving = 1,
		received = 2,
		done_receiving = 3,
		sending = 4,
		sent = 5,
		done_sending = 6
	};

	enum Command {
		start = 0,
		pause = 1,
		kick = 2
	};
}