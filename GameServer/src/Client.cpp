#include "Client.h"
#include <iostream>

Client::Client() {
	id_ = -1;
	alive_ = true;
	online_ = true;
}

Client::Client(const SOCKET socket, SharedMemory* shared_memory, const int id) : id_(id), socket_(socket) {
	alive_ = true;
	online_ = true;
	sharedMemory_ = shared_memory;

	std::cout << "CLIENT> Client#" << socket << " was assigned ID: " << id << std::endl;
}

Client::~Client() {
	// Terminate client thread
	std::terminate();
}

void Client::Loop() {
	while (online_) {
		Receive();
		std::cout << "INITIALIZING SENDING" << std::endl;
		Send();
		std::cout << "CLIENT> looped" << std::endl;
	}
	std::cout << "CLIENT> Thread " << id_ << " exited the loop" << std::endl;
	// Delete self
	delete this;
}

void Client::Receive() {
	while (true) {
		std::cout << "CLIENT> RECEIVING LOOP" << std::endl;
		// Perform receiving operation if serverApp is ready and
		// the client has not already performed it
		if (sharedMemory_->GetServerState() == State::receiving &&
			clientState_ != State::receiving) {

			std::cout << "CLIENT> RECEIVING" << std::endl;
			char incoming[1024];

			// Clear the storage before usage
			ZeroMemory(incoming, 1024);

			// Receive 
			const int bytes = recv(socket_, incoming, 1024, 0);

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
			std::cout << "CLIENT> RECEIVING (ADD)" << std::endl;
			clientState_ = State::receiving;
			sharedMemory_->ClientState(clientState_);
			std::cout << "CLIENT> RECEIVING (STATE)" << std::endl;

			std::cout << "SERVER> Socket: " << id_ << " has received" << std::endl;
			return;
		}	
	}
}

void Client::Send() {
	std::cout << "CLIENT> SENDING STATE" << std::endl;
	while (true) {
		//std::cout << "Server state: " << sharedMemory_->GetServerState() << std::endl;
		// Perform send operation if serverApp is ready and
		// the client has not already performed it
		if (sharedMemory_->GetServerState() == State::sending &&
			clientState_ != State::sending) {
			std::cout << "CLIENT> SENDING PERFORMED" << std::endl;
			std::string outgoing = (alive_ ? "A" : "D");
			std::cout << "CLIENT> SENDING" << std::endl;
			// Iterate through all clients
			std::string clientCoordinates = "<";
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

			// Client ready
			clientState_ = State::sending;
			sharedMemory_->ClientState(clientState_);
			std::cout << "CLIENT> SENDING FINISHED" << std::endl;
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

	std::cout << "CLIENT> Incoming message: " << command << std::endl;

	// Check if character has died
	if (command[0] == 'D' && alive_) {
		std::cout << "CLIENT> Client #" << socket_ << " died" << std::endl;
		alive_ = false;
	}

	// Strip player position
	while (std::regex_search(copy, mainMatcher, main)) {
		std::cout << "CLIENT> Regex" << std::endl;
		int x = 0, y = 0;

		for (auto segment : mainMatcher) {
			x = std::stoi(segment.str().substr(0, segment.str().find(":")));
			y = std::stoi(segment.str().substr(segment.str().find(":") + 1));
		}

		// Insert coordinate to list
		int coordinate[] = {x, y};
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

		std::cout << "CLIENT> Client #" << id_ << " was dropped" << std::endl;
	}
}


