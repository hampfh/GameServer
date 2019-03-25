#pragma once
#include <mutex>
#include "pch.h"

enum State {
	awaiting = 1,
	receiving = 2,
	sending = 3
};

class SharedMemory {
public:
	SharedMemory();
	~SharedMemory();

	void ClientState(State client_state);

	void Add(std::vector<std::vector<int>> client_coordinates);
	std::vector<std::vector<std::vector<int>>> GetCoordinates() const { return coordinates_; };
	void Reset();

	void SetState(State new_state);
	State GetServerState() const { return serverState_; };
	void SetConnectedClients(int connected_clients);
	int GetConnectedClients() const { return connectedClients_; };
	int GetReadyClients() const { return clientsReady_; };

	fd_set* GetSockets() { return &sockets_; };
	void AddSocket(SOCKET new_socket);
	void AddSocketList(fd_set list);

private:
	fd_set sockets_;
	State serverState_;

	int clientsReady_;
	int connectedClients_;

	std::mutex mutex_;

	std::vector<std::vector<std::vector<int>>> coordinates_;
};

