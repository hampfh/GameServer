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
	coordinates_.clear();
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
	sharedMemory_->AppendAdded(socket_, addedCoordinates_);
	sharedMemory_->AppendRemoved(socket_, removedCoordinates_);
	sharedMemory_->Ready();
	clientState_ = receiving;
}

void Client::Send() {
	std::string outgoing = (alive_ ? "A" : "D");

	// Iterate through all clients
	std::string clientCoordinates = "<";
	auto addedCoordinates = sharedMemory_->GetAdded();
	auto removedCoordinates = sharedMemory_->GetRemoved();

	for (auto& client : addedCoordinates) {
		// Client won't include itself in the response
		if (client[0][0] == static_cast<int>(socket_)) {
			continue;
		}
		// Remove header
		client.erase(client.begin());

		clientCoordinates.append("a{|");
		for (auto &coordinate : client) {
			clientCoordinates.append(
				std::to_string(coordinate[0]) +
				":" +
				std::to_string(coordinate[1])
			);
			clientCoordinates.append("|");
		}
		clientCoordinates.append("}");

		// Add client coordinates to the outgoing message
		outgoing.append(clientCoordinates);
		// Reset client string
		clientCoordinates = "";
	}

	for (auto& client : removedCoordinates) {
		// Client won't include itself in the response
		if (client[0][0] == static_cast<int>(socket_)) {
			continue;
		}
		// Remove header
		client.erase(client.begin());

		clientCoordinates.append("r{|");
		for (auto &coordinate : client) {
			clientCoordinates.append(
				std::to_string(coordinate[0]) +
				":" +
				std::to_string(coordinate[1])
			);
			clientCoordinates.append("|");
		}
		clientCoordinates.append("}>");

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

	std::vector<std::vector<int>> addList;
	std::vector<std::vector<int>> removeList;

	// Convert incoming to command
	const std::string command = std::string(incoming, bytes);
	std::string copy = command;

	// Match "add" coordinates
	const std::regex regexAdd("a\\{\\|([^}]+)\\|\\}");
	// Match "remove" coordinates
	const std::regex regexRemove("r\\{\\|([^}]+)\\|\\}");

	std::smatch mainMatcher;

	// Check if character has died
	if (command[0] == 'D' && alive_) {
		log_->warn("Client#" + std::to_string(socket_) + " died");
		alive_ = false;
	}

	// Find the coordinates to append
	while (std::regex_search(copy, mainMatcher, regexAdd)) {
		for (auto addSegment : mainMatcher) {
			// Strip out coordinates
			addList = StripCoordinates(addSegment.str());
		}
		copy = mainMatcher.suffix().str();
	}

	copy = command;

	// Find the coordinates to remove
	while (std::regex_search(copy, mainMatcher, regexRemove)) {
		for (auto removeSegment : mainMatcher) {
			// Strip out coordinates
			removeList = StripCoordinates(removeSegment.str());
		}
		copy = mainMatcher.suffix().str();
	}

	// Add changes of coordinates to client	
	addedCoordinates_ = addList;
	removedCoordinates_ = removeList;

	// Perform adds on coordinates_
	// Make space for new vector
	coordinates_.reserve(addList.size());
	// Insert the new vector to all coordinates
	coordinates_.insert(coordinates_.end(), addList.begin(), addList.end());

	// Perform removes on coordinates_
	for (int i = 0; i < coordinates_.size(); i++) {
		for (const auto& toRemove : removeList) {
			if (coordinates_[i] == toRemove) {
				coordinates_.erase(coordinates_.begin() + i);
			}
		}
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

		spdlog::drop("Client#" + std::to_string(socket_));
		sharedMemory_->DropSocket(socket_);
	}
}

std::vector<std::vector<int>> Client::StripCoordinates(std::string string) const {
	std::vector<std::vector<int>> coordinates;
	std::smatch matcher;
	const std::regex main("\\d+:\\d+");

	// Get coordinates form string
	// Strip player position
	while (std::regex_search(string, matcher, main)) {
		int x = 0, y = 0;

		for (auto segment : matcher) {
			x = std::stoi(segment.str().substr(0, segment.str().find(":")));
			y = std::stoi(segment.str().substr(segment.str().find(":") + 1));
		}

		std::vector<int> coordinate = { x, y };
		coordinates.push_back(coordinate);

		// Remove this coordinate from the remaining coordinates
		string = matcher.suffix().str();
	}
	return coordinates;
}


