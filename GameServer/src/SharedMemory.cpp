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

void SharedMemory::AppendClientCommands(const SOCKET socket, std::vector<int[2]> client_coordinates) {
	while (true) {
		if (addCoordinateMtx_.try_lock()) {

			// Add socket id to command
			for (auto command : client_coordinates) {
				command[0] = socket;
			}
			
			// Add the clients coordinates to shared memory
			clientCommands_.push_back(client_coordinates);

			// Marks the clients as ready
			addCoordinateMtx_.unlock();
			return;
		}
		std::this_thread::sleep_for(std::chrono::microseconds(100));
	}
}

void SharedMemory::Reset() {
	clientCommands_.clear();
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
