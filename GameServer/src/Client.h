#pragma once
#include "SharedMemory.h"

class Client {
public:
	Client(SOCKET socket, SharedMemory* shared_memory, int id);
	~Client();
	void Loop();
	void Receive();
	void Send();
	void CoreCallListener();
	std::vector<std::string> Interpret(std::string string) const;
	void Drop() const;

	void SetSocket(SOCKET socket);
	void SetId(int id);
	void SetInterval(std::chrono::microseconds microseconds);
private:
	int id_;
	bool isOnline_;
	SOCKET socket_;
	SharedMemory* sharedMemory_;
	std::chrono::microseconds loopInterval_;

	std::shared_ptr<spdlog::logger> log_;

	State clientState_;

	std::string clientCommand_;
	std::string pendingSend_;
};

