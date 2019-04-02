#pragma once
#include <mutex>
#include "pch.h"

enum State {
	none = 0,
	awaiting = 1,
	receiving = 2,
	sending = 3
};

class SharedMemory {
public:
	SharedMemory(std::shared_ptr<spdlog::sinks::rotating_file_sink<std::mutex>> shared_file_sink);
	~SharedMemory();

	void Ready();

	void AppendClientCommands(SOCKET socket, std::vector<int[2]> client_coordinates);
	std::vector<std::vector<int[2]>> GetClientCommands() const { return clientCommands_; };
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
	std::mutex addCoordinateMtx_;
	std::mutex addSocketMtx_;
	std::mutex dropSocketMtx_;

	// First vector holds x any y for a coordinate
	// Second vector hold all coordinates
	// Third vector hold all socket and the command for each client
	std::vector<std::vector<int[2]>> clientCommands_;
public:
	const std::shared_ptr<spdlog::sinks::rotating_file_sink<std::mutex>> sharedFileSink;
};

