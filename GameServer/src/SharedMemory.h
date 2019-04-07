#pragma once
#include <mutex>
#include "pch.h"

enum State {
	none = 0,
	awaiting = 1,
	receiving = 2,
	sending = 3
};

enum Command {
	start = 0,
	kick = 1
};

class SharedMemory {
public:
	SharedMemory(std::shared_ptr<spdlog::sinks::rotating_file_sink<std::mutex>> shared_file_sink);
	~SharedMemory();

	void Ready();

	void AppendClientCommands(int id, std::string command);
	void AddCoreCall(int receiver, int command);
	void ResetCoreCall();
	void PerformedCoreCall();
	std::vector<std::string> GetClientCommands() const { return commandQueue_; };
	std::vector<std::vector<int>> GetCoreCall() const { return coreCall_; };
	void Reset();

	void SetState(State new_state);
	State GetServerState() const { return serverState_; };
	void SetConnectedClients(int connected_clients);
	int GetConnectedClients() const { return connectedClients_; };
	int GetReadyClients() const { return clientsReady_; };

	fd_set* GetSockets() { return &sockets_; };
	void AddSocket(SOCKET new_socket);
	void DropSocket(SOCKET socket);
	void AddSocketList(fd_set list);

private:
	fd_set sockets_;
	State serverState_;

	int clientsReady_;
	int connectedClients_;

	std::shared_ptr<spdlog::logger> log_;

	std::mutex clientStateMtx_;
	std::mutex addCommandMtx_;
	std::mutex addSocketMtx_;
	std::mutex dropSocketMtx_;
	std::mutex coreCallMtx_;

	std::vector<std::string> commandQueue_;
	std::vector<std::vector<int>> coreCall_;
	int coreCallPerformedCount_;
public:
	const std::shared_ptr<spdlog::sinks::rotating_file_sink<std::mutex>> sharedFileSink;
};

