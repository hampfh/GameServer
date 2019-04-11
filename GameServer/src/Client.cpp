#include "pch.h"
#include "Client.h"

Client::Client(const SOCKET socket, SharedMemory* shared_memory, const int id) : socket_(socket), sharedMemory_(shared_memory), id(id) {

	isOnline_ = true;
	state_ = none;

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
	clientCommand_.clear();
}

void Client::Loop() {
	while (isOnline_) {
		// Listen for calls from core
		CoreCallListener();

		// Perform send operation if serverApp is ready and
		// the client has not already performed it
		if (*lobbyState_ == sending &&
			state_ != sending) {
			log_->info("Started sending");
			Send();
		}
		// Perform receiving operation if serverApp is ready and
		// the client has not already performed it
		else if (*lobbyState_ == receiving &&
			state_ != receiving) {
			log_->info("Started receiving");
			Receive();
		}
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
		state_ = receiving;
		return;
	}

	clientCommand_ = std::string(incoming, strlen(incoming));

	// Encapsulate command inside a socket block
	clientCommand_.insert(0, "{" + std::to_string(id) + "|");
	clientCommand_.append("}");

	state_ = receiving;

}

void Client::Send() {
	// Append potential command from core
	std::string outgoing = pendingSend_;

	// Iterate through all clients
	auto queue = outgoingCommands_;

	for (auto& client : queue) {

		// Skip command if it comes from the client itself
		if (Interpret(client)[0] == std::to_string(id)) {
			continue;
		}

		outgoing.append(client);
	}

	// Send payload
	// TODO encode output using compressor tool
	send(socket_, outgoing.c_str(), static_cast<int>(outgoing.size()) + 1, 0);

	// Client ready
	state_ = sending;

	// Reset pending send
	pendingSend_.clear();
}

void Client::CoreCallListener() {

	// Get core call
	std::vector<std::vector<int>> coreCall = sharedMemory_->GetCoreCall();
	if (!coreCall.empty()) {
		pendingSend_.append("{0|");
		for (auto frame : coreCall) {
			// Check if call is meant for this client or is a broadcast
			if (frame[0] == 0 &&
				(frame[1] == id || frame[1] == 0)) {

				if (frame.size() != 3) {
					continue;
				}

				const int command = frame[2];

				// Interpret command
				if (command == Command::start) {
					pendingSend_.append("S");
				}
				else if (command == Command::kick) {
					isOnline_ = false;
				}
			}
		}

		pendingSend_.append("}");

		// Clear outgoing if no information
		if (pendingSend_.length() <= 4) {
			log_->warn("Cleared pendingSend");
			pendingSend_.clear();
		}
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
	if (socket_ != 0) {
		log_->info("Client#" + std::to_string(socket_) + " was dropped");

		spdlog::drop("Client#" + std::to_string(socket_));

		// Drop client globally
		sharedMemory_->DropSocket(socket_);

		// Add self to dropList in lobby
		dropList_->push_back(this->id);
	}
}

void Client::SetSocket(const SOCKET socket) {
	socket_ = socket;
}

void Client::SetId(const int id) {
	this->id = id;
}

void Client::SetInterval(const std::chrono::microseconds microseconds) {
	loopInterval_ = microseconds;
}

void Client::SetLobbyStateReference(State* lobby_state) {
	lobbyState_ = lobby_state;
}

void Client::SetLobbyDropReference(std::vector<int>* drop_list) {
	dropList_ = drop_list;
}

void Client::SetState(const State state) {
	state_ = state;
}