#include "pch.h"
#include "SharedMemory.h"

SharedMemory::SharedMemory() {
	clientsReady_ = 0;
}


SharedMemory::~SharedMemory() {
	Reset();
}

void SharedMemory::ClientState(const State client_state) {
	while (true) {
		// If the clients state matches the servers then 
		// consider the client ready for next state
		if (client_state == serverState_ && mutex_.try_lock()) {
			clientsReady_++;
			mutex_.unlock();
			return;
		}
	}
}

void SharedMemory::Add(std::vector<std::vector<int>> client_coordinates) {
	while (true) {
		if (mutex_.try_lock()) {
			coordinates_.push_back(client_coordinates);
			mutex_.unlock();
			return;
		}
	}
}

void SharedMemory::Reset() {
	coordinates_.clear();
}

void SharedMemory::SetConnectedClients(const int connected_clients) {
	connectedClients_ = connected_clients;
}


void SharedMemory::SetState(const State new_state) {
	serverState_ = new_state;
	// Reset clients ready when changing state
	clientsReady_ = 0;
}

void SharedMemory::AddSocket(const SOCKET new_socket) {
	while (true) {
		if (mutex_.try_lock()) {
			// Add newClient to the socketList
			FD_SET(new_socket, &sockets_);
			mutex_.unlock();
			return;
		}
	}
}

void SharedMemory::AddSocketList(const fd_set list) {
	sockets_ = list;
}
