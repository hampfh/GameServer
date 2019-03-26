#include "pch.h"
#include "Client.h"

Client::Client() {
	id_ = -1;
	alive_ = true;
	online_ = true;
}

Client::Client(const SOCKET socket, SharedMemory* shared_memory, const int id) : id_(id), socket_(socket) {
	alive_ = true;
	online_ = true;
	sharedMemory_ = shared_memory;
	
	// Assign logger
	log_ = spdlog::get("Client");

	log_->info("Client#" + std::to_string(socket) + " was assigned ID: " + std::to_string(id));
}

Client::~Client() {
	// Terminate client thread
	//std::terminate();
}

void Client::Loop() {
	while (online_) {
		Receive();
		Send();
	}
	log_->info("Thread " + std::to_string(id_) + " exited the loop");
	// Delete self
	delete this;
}

void Client::Receive() {
	while (true) {
		// Perform receiving operation if serverApp is ready and
		// the client has not already performed it
		if (sharedMemory_->GetServerState() == State::receiving &&
			clientState_ != State::receiving) {

			char incoming[1024];

			// Clear the storage before usage
			ZeroMemory(incoming, 1024);

			// Receive 
			const int bytes = recv(socket_, incoming, 1024, 0);

			log_->info("Receiving: " + std::string(incoming, sizeof(incoming)));

			// Check if client responds
			if (bytes <= 0) {
				// Disconnect client
				online_ = false;
				Drop();
				return;
			}

			// Interpret response
			Interpret(incoming, bytes);

			// Add to shared memory
			sharedMemory_->Add(coordinates_);

			// Clear client coords'
			coordinates_.clear();
			
			clientState_ = State::receiving;
			sharedMemory_->ClientState(clientState_);

			return;
		}	
	}
}

void Client::Send() {
	while (true) {
		// Perform send operation if serverApp is ready and
		// the client has not already performed it
		if (sharedMemory_->GetServerState() == State::sending &&
			clientState_ != State::sending) {
			std::string outgoing = (alive_ ? "A" : "D");

			// Iterate through all clients
			std::string clientCoordinates = "";
			auto coordinates = sharedMemory_->GetCoordinates();

			for (auto &client : sharedMemory_->GetCoordinates()) {
				clientCoordinates.append("<");
				for (auto &coordinate : client) {
					// Add coordinate separator
					clientCoordinates.append("|");
					// Append separator

					clientCoordinates.append(
						std::to_string(coordinate[0]) +
						":" +
						std::to_string(coordinate[1])
					);
				}
				clientCoordinates.append("|>");

				// Add client coordinates to the outgoing message
				outgoing.append(clientCoordinates);
				// Reset client string
				clientCoordinates = "";
			}

			// Send payload
			send(socket_, outgoing.c_str(), outgoing.size() + 1, 0);

			log_->info("Outgoing: " + outgoing);

			// Client ready
			clientState_ = State::sending;
			sharedMemory_->ClientState(clientState_);
			return;
		}
	}
}

void Client::Interpret(char* incoming, const int bytes) {
	// Convert incoming to command
	std::string command = std::string(incoming, bytes);
	std::string copy = command;
	const std::regex main("\\d+:\\d+");
	std::smatch mainMatcher;

	// Check if character has died
	if (command[0] == 'D' && alive_) {
		log_->warn("Client#" + std::to_string(socket_) + " died");
		alive_ = false;
	}

	// Strip player position
	while (std::regex_search(copy, mainMatcher, main)) {
		int x = 0, y = 0;

		for (auto segment : mainMatcher) {
			x = std::stoi(segment.str().substr(0, segment.str().find(":")));
			y = std::stoi(segment.str().substr(segment.str().find(":") + 1));
		}

		std::vector<int> coordinate = {x, y};
		coordinates_.push_back(coordinate);

		// Remove this coordinate from the remaining coordinates
		copy = mainMatcher.suffix().str();
	}
}


void Client::SetSocket(const SOCKET socket) {
	socket_ = socket;
}

void Client::SetId(const int id) {
	id_ = id;
}

void Client::Drop() const {
	if (socket_ != 0) {
		// Remove socket from socketList
		FD_CLR(socket_, sharedMemory_->GetSockets());

		// Clear the socket
		closesocket(socket_);

		log_->info("Client#" + std::to_string(id_) + " was dropped");
	}
}


