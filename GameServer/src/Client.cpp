#include "pch.h"
#include "Client.h"

Client::Client(const SOCKET socket, SharedMemory* shared_memory, const int id) : id_(id), socket_(socket), sharedMemory_(shared_memory) {
	isOnline_ = true;
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
	while (isOnline_) {
		// Listen for calls from core
		CoreCallListener();

		// Perform send operation if serverApp is ready and
		// the client has not already performed it
		if (sharedMemory_->GetState() == sending &&
			clientState_ != sending) {
			Send();
		}
		// Perform receiving operation if serverApp is ready and
		// the client has not already performed it
		else if (sharedMemory_->GetState() == receiving &&
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
		isOnline_ = false;
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
	// Append potential command from core
	std::string outgoing = pendingSend_;

	// Iterate through all clients
	auto queue = sharedMemory_->GetClientCommands();

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
				(frame[1] == id_ || frame[1] == 0)) {

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

		sharedMemory_->PerformedCoreCall();
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