#include "pch.h"
#include "SharedMemory.h"

SharedMemory::SharedMemory(const std::shared_ptr<spdlog::sinks::rotating_file_sink<std::mutex>> shared_file_sink) : sharedFileSink(shared_file_sink) {
	clientsReady_ = 0;
	serverState_ = none;
	connectedClients_ = 0;
	coreCallPerformedCount_ = 0;

	// Setup memory logger
	std::vector<spdlog::sink_ptr> sinks;
	sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
	sinks.push_back(shared_file_sink);
	log_ = std::make_shared<spdlog::logger>("Shared Memory", begin(sinks), end(sinks));
	log_->set_pattern("[%a %b %d %H:%M:%S %Y] [%L] %^%n: %v%$");
	register_logger(log_);
}

SharedMemory::~SharedMemory() {
	ResetCommandQueue();
}

void SharedMemory::Ready() {
	while (true) {
		// If the clients state matches the servers then 
		// consider the client ready for next state
		if (clientStateMtx_.try_lock()) {
			clientsReady_++;
			clientStateMtx_.unlock();
			return;
		}
		std::this_thread::sleep_for(std::chrono::microseconds(100));
	}
}

void SharedMemory::AppendClientCommands(const int id, std::string command) {
	while (true) {
		if (addCommandMtx_.try_lock()) {

			// Encapsulate command inside a socket block
			command.insert(0, "{" + std::to_string(id) + "|");
			command.append("}");
			
			// Add the clients coordinates to shared memory
			commandQueue_.push_back(command);

			// TODO Add a storage that saves all past commands to make it possible for clients to connect later

			// Marks the clients as ready
			addCommandMtx_.unlock();
			return;
		}
		std::this_thread::sleep_for(std::chrono::microseconds(100));
	}
}

void SharedMemory::AddCoreCall(const int receiver, const int command) {
	// Create and append call
	// Sender, Receiver, Command
	coreCall_.push_back({ 0, receiver, command });
}

void SharedMemory::PerformedCoreCall() {
	while (true) {
		if (coreCallMtx_.try_lock()) {
			coreCallPerformedCount_++;
			if (coreCallPerformedCount_ == connectedClients_) {
				ResetCoreCall();
			}
			coreCallMtx_.unlock();
			return;
		}
		std::this_thread::sleep_for(std::chrono::microseconds(100));
	}
}

void SharedMemory::ResetCoreCall() {
	coreCallPerformedCount_ = 0;
	coreCall_.clear();
}

void SharedMemory::AddSocket(const SOCKET new_socket) {
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
	}
}

void SharedMemory::DropSocket(const SOCKET socket) {
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
	}
}

void SharedMemory::ResetCommandQueue() {
	commandQueue_.clear();
}

void SharedMemory::SetConnectedClients(const int connected_clients) {
	connectedClients_ = connected_clients;
}


void SharedMemory::SetState(const State new_state) {
	serverState_ = new_state;
	// Reset clients ready when changing state
	clientsReady_ = 0;
}

void SharedMemory::SetSockets(const fd_set list) {
	sockets_ = list;
}
