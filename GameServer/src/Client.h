#pragma once
#include "SharedMemory.h"

class Client {
public:
	Client();
	Client(SOCKET socket, SharedMemory* shared_memory, int id);
	~Client();
	void Loop();
	void Receive();
	void Send();
	void Interpret(char* incoming, const int bytes);
	void Drop() const;

	void SetSocket(SOCKET socket);
	void SetId(int id);
private:
	int id_;
	int* clientCount_;
	bool online_;
	bool alive_;
	SOCKET socket_;
	SharedMemory* sharedMemory_;

	std::shared_ptr<spdlog::logger> fileClient_;
	std::shared_ptr<spdlog::logger> conClient_;

	State clientState_;

	std::vector<std::vector<int>> coordinates_;
};

