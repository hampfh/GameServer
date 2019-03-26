#pragma once
#include "Client.h"
#include "SharedMemory.h"

class Core {
public:
	Core();
	~Core();
	void Execute();
	void Loop();
	void CleanUp();

	void InitializeSending() const;
	void InitializeReceiving();
	void Interpreter();
private:
	bool running_;

	SOCKET listening_;

	int seed_;
	int clientId_;
	int serverState_ = 0;

	std::shared_ptr<spdlog::logger> log_;

	timeval timeInterval_;

	fd_set workingSet_;

	SharedMemory* sharedMemory_;
};
