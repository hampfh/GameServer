#pragma once
#include <vector>
#include <winsock2.h>
#include <regex>
#include <thread>
#include <iostream>
#include <ctime>
#include <string>

// Logging library
#include "spdlog/spdlog.h";
#include "spdlog/sinks/stdout_sinks.h";
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

// String compressor
//#include "cpp-base64-master/base64.h";

#include "cpp-compressor.h"

#pragma comment(lib,"WS2_32")
