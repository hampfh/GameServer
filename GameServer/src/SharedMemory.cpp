#include "pch.h"
#include "SharedMemory.h"

SharedMemory::SharedMemory() {
	clientsReady_ = 0;
	serverState_ = none;
	connectedClients_ = 0;
}

SharedMemory::~SharedMemory() {
	Reset();
}

void SharedMemory::ClientState(const State client_state) {
	while (true) {
		// If the clients state matches the servers then 
		// consider the client ready for next state
		if (client_state == serverState_ && clientStateMtx_.try_lock()) {
			clientsReady_++;
			clientStateMtx_.unlock();
			return;
		}
		std::cout << "1 WAIT" << std::endl;
		std::this_thread::sleep_for(std::chrono::microseconds(100));
	}
}

void SharedMemory::Add(std::vector<std::vector<int>> client_coordinates) {
	while (true) {
		if (addCoordinateMtx_.try_lock()) {
			coordinates_.push_back(client_coordinates);
			addCoordinateMtx_.unlock();
			return;
		}
		std::cout << "2 WAIT" << std::endl;
		std::this_thread::sleep_for(std::chrono::microseconds(100));
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
		if (addSocketMtx_.try_lock()) {
			// Add newClient to the socketList
			FD_SET(new_socket, &sockets_);
			addSocketMtx_.unlock();
			return;
		}
		std::cout << "3 WAIT" << std::endl;
		std::this_thread::sleep_for(std::chrono::microseconds(100));
	}
}

void SharedMemory::AddSocketList(const fd_set list) {
	sockets_ = list;
}
