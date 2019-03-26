#include "pch.h"
#include "Client.h"

Client::Client(const SOCKET socket, SharedMemory* shared_memory, const int id) : id_(id), socket_(socket) {
	alive_ = true;
	online_ = true;
	sharedMemory_ = shared_memory;
	clientState_ = none;

	loopInterval_ = std::chrono::microseconds(1000);
	
	// Setup client logger
	std::vector<spdlog::sink_ptr> sinks;
	sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
	sinks.push_back(sharedMemory_->sharedFileSink);
	log_ = std::make_shared<spdlog::logger>("Client#" + std::to_string(socket), begin(sinks), end(sinks));
	log_->set_pattern("[%a %b %d %H:%M:%S %Y] [%Lf] %^%n: %v%$");
	register_logger(log_);
	
	log_->info("Assigned ID: " + std::to_string(id));
}

Client::~Client() {
	// Terminate client thread
	try {
		//std::terminate();
	} catch (...) {
		log_->error("Termination problem: ");
	}
}

void Client::Loop() {
	while (online_) {
		// Perform send operation if serverApp is ready and
		// the client has not already performed it
		if (sharedMemory_->GetServerState() == sending &&
			clientState_ != sending) {
			Send();
		}
		// Perform receiving operation if serverApp is ready and
		// the client has not already performed it
		else if (sharedMemory_->GetServerState() == receiving &&
			clientState_ != receiving) {
			Receive();
		}
		// Thread sleep
		std::this_thread::sleep_for(loopInterval_);
	}
	log_->info("Thread " + std::to_string(id_) + " exited the loop");
	Drop();
	// Delete self
	delete this;
}

void Client::Receive() {
	char incoming[1024];

	// Clear the storage before usage
	ZeroMemory(incoming, 1024);

	// Receive 
	const int bytes = recv(socket_, incoming, 1024, 0);

	// Check if client responds
	if (bytes <= 0) {
		// Disconnect client
		online_ = false;
		return;
	}

	// Interpret response
	Interpret(incoming, bytes);

	// Add to shared memory and mark as ready
	sharedMemory_->Add(coordinates_);
	clientState_ = receiving;

	// Clear client coordinates
	coordinates_.clear();
}

void Client::Send() {
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
	// TODO encode output using compressor tool
	send(socket_, outgoing.c_str(), outgoing.size() + 1, 0);

	// Client ready
	clientState_ = sending;
	sharedMemory_->Ready();

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

void Client::SetInterval(const std::chrono::microseconds microseconds) {
	loopInterval_ = microseconds;
}

void Client::Drop() const {
	if (socket_ != 0) {
		log_->info("Client#" + std::to_string(socket_) + " was dropped");

		sharedMemory_->DropSocket(socket_);
	}
}


