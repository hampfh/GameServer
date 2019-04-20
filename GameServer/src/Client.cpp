#include "pch.h"
#include "Client.h"

Client::Client(const SOCKET socket, SharedMemory* shared_memory, const int id, const int lobby_id) : 
socket_(socket), sharedMemory_(shared_memory), lobbyId(lobby_id), id(id) {

	isOnline_ = true;
	state_ = none;
	lastState_ = none;
	paused_ = false;

	loopInterval_ = std::chrono::microseconds(1000);

	// Setup client logger
	std::vector<spdlog::sink_ptr> sinks;
	sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
	sinks.push_back(sharedMemory_->GetFileSink());
	log_ = std::make_shared<spdlog::logger>("Client#" + std::to_string(socket), begin(sinks), end(sinks));
	log_->set_pattern("[%a %b %d %H:%M:%S %Y] [%L] %^%n: %v%$");
	register_logger(log_);

	log_->info("Assigned ID: " + std::to_string(id));
}

Client::~Client() {
	isOnline_ = false;
	log_->info("I was dropped");
	spdlog::drop("Client#" + std::to_string(socket_));
}

void Client::Loop() {
	while (isOnline_) {
		// Listen for calls from core
		CoreCallListener();

		// Perform send operation if serverApp is ready and
		// the client has not already performed it
		if (lobbyMemory_->GetState() == sending &&
			lastState_ != sending) { Send(); }
			// Perform receiving operation if serverApp is ready and
			// the client has not already performed it
		else if (lobbyMemory_->GetState() == receiving &&
			lastState_ != receiving) { Receive(); }
		// Thread sleep
		std::this_thread::sleep_for(loopInterval_);
	}

	log_->info("Thread " + std::to_string(id) + " exited the loop");
	RequestDrop();

	// Object will be delete by it's parent (lobby)
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
		isOnline_ = false;
		log_->warn("Lost connection to client");
		// Tell other clients that this client has disconnected
		clientCommand_ = "{" + std::to_string(id) + "|D}";
		lastState_ = receiving;
		state_ = receiving;
		return;
	}

	clientCommand_ = std::string(incoming, strlen(incoming));

	// Only encapsulate if there is any content
	if (bytes > 1) {
		// Encapsulate command inside a socket block
		clientCommand_.insert(0, "{" + std::to_string(id) + "|");
		clientCommand_.append("}");
	}
	else { clientCommand_.clear(); }

	lastState_ = receiving;

	// Ready call for client
	state_ = receiving;

}

void Client::Send() {
	// Append potential command from core
	std::string outgoing = pendingSend_;

	// Iterate through all clients
	auto queue = outgoingCommands_;

	for (auto& client : queue) {
		// Skip command if it comes from the client itself
		if (Interpret(client)[0] == std::to_string(id)) { continue; }

		outgoing.append(client);
	}

	// Send payload
	// TODO encode output using compressor tool
	send(socket_, outgoing.c_str(), static_cast<int>(outgoing.size()) + 1, 0);

	// Client ready
	lastState_ = sending;
	state_ = sending;

	// Reset pending send
	pendingSend_.clear();
}

void Client::CoreCallListener() {

	// Get core call
	if (!coreCall_.empty()) {
		pendingSend_.append("{0|");
		for (auto frame : coreCall_) {
			// Check if call is meant for this client or is a broadcast
			if (frame[0] == 0 &&
				(frame[1] == lobbyId || frame[1] == 0) &&
				(frame[2] == id || frame[2] == 0)) {
				if (frame.size() != 4) {
					log_->warn("Bad formatted call, ignoring");
					continue;
				}

				const int command = frame[3];

				// Interpret command

				// Send start command to client
				if (command == Command::start && lobbyId != 1) { pendingSend_.append("S"); }
				else if (command == Command::kick) { isOnline_ = false; }
			}
		}

		pendingSend_.append("}");

		// Clear outgoing if no information
		if (pendingSend_.length() <= 4) { pendingSend_.clear(); }

		coreCall_.clear();
	}
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

void Client::RequestDrop() const {

	// Drop client globally
	sharedMemory_->DropSocket(socket_);

	// Add self to dropList in lobby
	lobbyMemory_->AddDrop(this->id);
}

void Client::End() { isOnline_ = false; }

void Client::DropLobbyConnections() {

	lobbyId = -1;
	lobbyMemory_ = nullptr;
}

void Client::SetCoreCall(std::vector<int> coreCall) { coreCall_.push_back(coreCall); }

void Client::SetSocket(const SOCKET socket) { socket_ = socket; }

void Client::SetId(const int id) { this->id = id; }

void Client::SetInterval(const std::chrono::microseconds microseconds) { loopInterval_ = microseconds; }

void Client::SetMemory(SharedLobbyMemory* lobby_memory) { lobbyMemory_ = lobby_memory; }

void Client::SetPause(const bool pause) { paused_ = pause; };

void Client::SetState(const State state) { state_ = state; }

void Client::SetPrevState(const State state) { lastState_ = state; };

void Client::SetOutgoing(const std::vector<std::string> outgoing) { outgoingCommands_ = outgoing; }
