#include "pch.h"
#include "SharedMemory.h"

SharedMemory::SharedMemory(const std::shared_ptr<spdlog::sinks::rotating_file_sink<std::mutex>> shared_file_sink) : sharedFileSink(shared_file_sink) {
	clientsReady_ = 0;
	serverState_ = none;
	connectedClients_ = 0;

	// Setup memory logger
	std::vector<spdlog::sink_ptr> sinks;
	sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
	sinks.push_back(shared_file_sink);
	log_ = std::make_shared<spdlog::logger>("Shared Memory", begin(sinks), end(sinks));
	log_->set_pattern("[%a %b %d %H:%M:%S %Y] [%Lf] %^%n: %v%$");
	register_logger(log_);
}

SharedMemory::~SharedMemory() {
	Reset();
}

void SharedMemory::Ready() {

	int temp = 0;

	while (true) {
		// If the clients state matches the servers then 
		// consider the client ready for next state
		if (clientStateMtx_.try_lock()) {
			clientsReady_++;
			clientStateMtx_.unlock();
			return;
		}
		std::this_thread::sleep_for(std::chrono::microseconds(100));
		temp++;
	}
}

void SharedMemory::AppendAdded(const SOCKET socket, std::vector<std::vector<int>> client_coordinates) {
	while (true) {
		if (addCoordinateMtx_.try_lock()) {
			// Add socket identification in beginning
			const std::vector<int> header = {static_cast<int>(socket), 0 };
			client_coordinates.insert(client_coordinates.begin(), 1, header);

			// Add the clients coordinates to shared memory
			added_.push_back(client_coordinates);

			// Marks the clients as ready
			addCoordinateMtx_.unlock();
			return;
		}
		std::this_thread::sleep_for(std::chrono::microseconds(100));
	}
}

void SharedMemory::AppendRemoved(const SOCKET socket, std::vector<std::vector<int>> client_coordinates) {
	while (true) {
		if (addCoordinateMtx_.try_lock()) {
			// Add socket identification in beginning
			const std::vector<int> header = { static_cast<int>(socket), 0 };
			client_coordinates.insert(client_coordinates.begin(), 1, header);

			// Add the clients coordinates to shared memory
			removed_.push_back(client_coordinates);

			// Marks the clients as ready
			addCoordinateMtx_.unlock();
			return;
		}
		std::this_thread::sleep_for(std::chrono::microseconds(100));
	}
}

void SharedMemory::Reset() {
	added_.clear();
	removed_.clear();
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
	int temp = 0;
	while (true) {
		if (addSocketMtx_.try_lock()) {
			// Add newClient to the socketList
			FD_SET(new_socket, &sockets_);
			// Increase connected clients number
			connectedClients_++;
			addSocketMtx_.unlock();
			return;
		}
		std::this_thread::sleep_for(std::chrono::microseconds(100));
		temp++;
	}
}

void SharedMemory::DropSocket(const SOCKET socket) {
	int temp = 0;
	while (true) {
		if (dropSocketMtx_.try_lock()) {
			// Decrease online clients
			connectedClients_--;
			// Clear the socket
			closesocket(socket);
			// Remove socket from socketList
			FD_CLR(socket, &sockets_);
			dropSocketMtx_.unlock();
			return;
		}
		std::this_thread::sleep_for(std::chrono::microseconds(100));
		temp++;
	}
}

void SharedMemory::AddSocketList(const fd_set list) {
	sockets_ = list;
}
