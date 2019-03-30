#pragma once
#include "SharedMemory.h"

class Client {
public:
	Client(SOCKET socket, SharedMemory* shared_memory, int id);
	~Client();
	void Loop();
	void Receive();
	void Send();
	void Interpret(char* incoming, const int bytes);
	void Drop() const;
	std::vector<std::vector<int>> StripCoordinates(std::string string) const;

	void SetSocket(SOCKET socket);
	void SetId(int id);
	void SetInterval(std::chrono::microseconds microseconds);
private:
	int id_;
	bool online_;
	bool alive_;
	SOCKET socket_;
	SharedMemory* sharedMemory_;
	std::chrono::microseconds loopInterval_;

	std::shared_ptr<spdlog::logger> log_;

	State clientState_;

	std::vector<std::vector<int>> coordinates_;
	std::vector<std::vector<int>> addedCoordinates_;
	std::vector<std::vector<int>> removedCoordinates_;
};

