#pragma once
#include "pch.h"
#include "utilities.h"
#include "master_lobby.h"

/**
    Core.h
    Purpose: Main thread. This thread controls all other threads and delegates work 

    @author Hampus Hallkvist
    @version 0.3 07/05/2019
*/

namespace hgs {

	class Core {
	private:
		bool running_ = true;

		SOCKET rconListener_;
		fd_set rconSockets_;
		int rconConnections_ = 0;
		int rconIndex_ = 0;

		SOCKET listener_;
		fd_set sockets_;
		int connections_ = 0;
		int clientIndex_ = 0;

		MasterLobby master_;

		std::mutex callInterpreter_;

		unsigned int seed_;

		Configuration conf_;

		std::shared_ptr<spdlog::logger> log_;
		std::shared_ptr<spdlog::sinks::rotating_file_sink<std::mutex>> fileSink_;

	public:
		bool ready;

		Core();
		~Core();

		std::shared_ptr<spdlog::sinks::rotating_file_sink<std::mutex>> SetupSpdlog() const;
		Configuration SetupConfig() const;
		SOCKET SetupWinSock();
		SOCKET SetupRcon() const;
		void SetupSessionDir() const;

		void Execute();
		void CleanUp() const;
		void Loop();
		void AcceptNewConnections(int select_result, int rcon_select_result, fd_set workingSet, fd_set rconWorkingSet);
		void AddConnection(SOCKET socket);
		void DropConnection(SOCKET socket);
		void BroadcastCoreCall(int lobby, int receiver, int command) const;
		void ConsoleThread();
		std::pair<int, std::string> ServerCommand(std::string& command);
		std::shared_ptr<spdlog::sinks::rotating_file_sink<std::mutex>> GetFileSink() const { return fileSink_; };
		std::pair<int, std::string> Interpreter(std::string& input);
	};

}