#include "pch.h"
#include "Client.h"

Client::Client(const SOCKET socket, SharedMemory* shared_memory, const int id) : id_(id), socket_(socket), sharedMemory_(shared_memory) {
	alive_ = true;
	online_ = true;
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
	clientCommand_.clear();
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
		log_->warn("Lost connection to client");
		// Tell other clients that this client has disconnected
		sharedMemory_->AppendClientCommands(id_, "D");
		clientCommand_.clear();
		sharedMemory_->Ready();
		return;
	}

	// Interpret response

	clientCommand_ = std::string(incoming, strlen(incoming));

	// Add to shared memory and mark as ready
	sharedMemory_->AppendClientCommands(id_, clientCommand_);

	clientCommand_.clear();

	sharedMemory_->Ready();
	clientState_ = receiving;
}

void Client::Send() {
	std::string outgoing;

	// Iterate through all clients
	auto queue = sharedMemory_->GetClientCommands();
	std::string coreCall = sharedMemory_->GetCoreCall();
	if (!coreCall.empty()) {
		outgoing = coreCall;
		sharedMemory_->PerformedCoreCall();
	}

	for (auto& client : queue) {

		// Skip command if it comes from the client itself
		if (Interpret(client)[0] == std::to_string(id_)) {
			continue;
		}

		outgoing.append(client);
	}

	// Send payload
	// TODO encode output using compressor tool
	send(socket_, outgoing.c_str(), static_cast<int>(outgoing.size()) + 1, 0);

	// Client ready
	clientState_ = sending;
	sharedMemory_->Ready();

}

std::vector<std::string> Client::Interpret(std::string string) const {

	std::vector<std::string> matches;

	const std::regex regexCommand("[^\\|{}\\[\\]]+");
	std::smatch mainMatcher;

	while (std::regex_search(string, mainMatcher, regexCommand)) {
		for (auto addSegment : mainMatcher) {
			// Append the commands to the list
			matches.push_back(addSegment);
		}
		string = mainMatcher.suffix().str();
	}

	return matches;
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


