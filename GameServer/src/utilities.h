#pragma once
#include "pch.h"

/**
    SharedMemory.h
    Purpose: This files hold further utility functions

    @author Hampus Hallkvist
    @version 0.3 28/05/2019
*/

namespace hgs {
	namespace utilities {
		bool IsInt(std::string& string);
		std::shared_ptr<spdlog::logger> SetupLogger(std::string& logger_name, std::shared_ptr<spdlog::sinks::rotating_file_sink<std::mutex>> file_sink);
		// Generate a random number using microsoft random
		int Random();
	}

	
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

	enum ClientCommand {
		
	};

	struct Configuration {
		int serverPort = NULL;
		int clockSpeed = NULL;
		timeval socketProcessingMax = timeval();
		int timeoutTries = NULL;
		float timeoutDelay = NULL;
		int maxConnections = NULL;
		bool rconEnable = NULL;
		int rconPort = NULL;
		std::string logPath;
		std::string sessionPath;
		std::string rconPassword;
		int rconMaxConnections = NULL;
		int lobbyMaxConnections = NULL;
		bool lobbySessionLogging = NULL;
		int lobbyStartIdAt = NULL;
		int clientStartIdAt = NULL;
	};
}