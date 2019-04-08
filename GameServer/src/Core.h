#pragma once
#include "Client.h"
#include "SharedMemory.h"

class Core {
public:
	Core();
	void SetupConfig();
	std::shared_ptr<spdlog::sinks::rotating_file_sink_mt> SetupLogging();
	void SetupWinSock();
	~Core();

	void Execute();
	void Loop();
	void CleanUp() const;

	void InitializeSending() const;
	void InitializeReceiving(int select_result);
	void Interpreter() const;
private:
	bool running_;

	SOCKET listening_;

	int clientId_;
	int maxConnections_;

	int seed_;
	State serverState_ = receiving;

	std::shared_ptr<spdlog::logger> log_;

	int timeoutTries_;
	float timeoutDelay_;

	timeval timeInterval_;

	fd_set workingSet_;

	SharedMemory* sharedMemory_;

	std::chrono::milliseconds clockSpeed_;
};

