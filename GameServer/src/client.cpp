#include "pch.h"
#include "client.h"
#include "utilities.h"

hgs::Client::Client(const SOCKET socket, const gsl::not_null<SharedMemory*> shared_memory, const int id, const int lobby_id) :
socket_(socket), comRegex_("[^\\|{}\\[\\]]+"), sharedMemory_(shared_memory), lobbyId(lobby_id), id(id) {

	isOnline_ = true;
	state_ = none;
	lastState_ = none;
	paused_ = false;
	attached_ = true;
	//upStreamCall_ = nullptr;

	loopInterval_ = std::chrono::microseconds(1000);

	// Setup client logger
	std::vector<spdlog::sink_ptr> sinks;
	sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
	sinks.push_back(sharedMemory_->GetFileSink());
	log_ = std::make_shared<spdlog::logger>("Client#" + std::to_string(socket), begin(sinks), end(sinks));
	log_->set_pattern("[%a %b %d %H:%M:%S %Y] [%L] %^%n: %v%$");
	register_logger(log_);

}

hgs::Client::~Client() {
	isOnline_ = false;
	log_->info("Dropped");
	spdlog::drop("Client#" + std::to_string(socket_));
	sharedMemory_->DropSocket(socket_);
}

void hgs::Client::Loop() {
	while (isOnline_) {
		// Listen for calls from core
		CoreCallListener();

		// Don't send and receive in transition states between lobbies
		if (lobbyMemory_ != nullptr) {
			// Perform send operation if serverApp is ready and
			// the client has not already performed it
			if (lobbyMemory_->GetState() == sending &&
				lastState_ != sending) {
				Send();
			}
			// Perform receiving operation if serverApp is ready and
			// the client has not already performed it
			else if (lobbyMemory_->GetState() == receiving &&
				lastState_ != receiving) {
				Receive();
			}
			std::this_thread::sleep_for(loopInterval_);
		}
	}

	if (attached_) {
		// Add self to dropList in lobby
		lobbyMemory_->AddDrop(this->id);

		while (attached_) {
			std::this_thread::sleep_for(std::chrono::milliseconds(250));
		}
	}

	delete this;
}

void hgs::Client::Receive() {

	state_ = receiving;

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
		state_ = received;
		return;
	}

	clientCommand_ = std::string(incoming, strlen(incoming));

	// Only encapsulate if there is any content
	if (bytes > 1) {
		if (IsApiCall(clientCommand_)) {
			PerformApiCall(clientCommand_);
			clientCommand_.clear();
		} else {
			// Encapsulate command inside a socket block
			clientCommand_.insert(0, "{" + std::to_string(id) + "|");
			clientCommand_.append("}");	
		}
	}
	else 
		clientCommand_.clear();

	lastState_ = receiving;

	// Ready call for client
	state_ = received;

}

void hgs::Client::Send() {
	state_ = sending;

	// Append potential command from core
	std::string outgoing = pendingSend_;

	// Iterate through all clients
	auto queue = outgoingCommands_;

	for (auto& client : queue) {
		// Skip command if it comes from the client itself
		if (Split(client)[0] == std::to_string(id)) { continue; }

		outgoing.append(client);
	}

	// Send payload
	// TODO encode output using compressor tool
	send(socket_, outgoing.c_str(), static_cast<int>(outgoing.size()) + 1, 0);

	// Client ready
	lastState_ = sending;
	state_ = sent;

	// Reset pending send
	pendingSend_.clear();
}

void hgs::Client::CoreCallListener() {

	// Get core call
	if (!coreCall_.empty()) {
		pendingSend_.append("{0|");
		for (auto frame : coreCall_) {
			if (frame.size() != 3) {
				log_->warn("Bad formatted call, ignoring");
				continue;
			}

			// Check if call is meant for this client or is a broadcast
			if ((frame[0] == lobbyId || frame[0] == 0) &&
				(frame[1] == id || frame[1] == 0)) {

				const int command = frame[2];

				// Interpret command

				// Send start command to client
				switch (command) {
					case Command::start:
						pendingSend_.append("S");
						break;
					case Command::pause:
						pendingSend_.append("P");
						break;
					case Command::kick:
						isOnline_ = false;
						break;
					default:
						break;
				}
			}
		}

		pendingSend_.append("}");

		// Clear outgoing if no information
		if (pendingSend_.length() <= 4) { pendingSend_.clear(); }

		coreCall_.clear();
	}
}

std::vector<std::string> hgs::Client::Split(std::string string) const {

	std::vector<std::string> matches;

	std::smatch mainMatcher;

	std::pair<bool, std::string> result;

	do {
		result = SplitFirst(string, mainMatcher, comRegex_);
		matches.push_back(result.second);

		string = mainMatcher.suffix().str();
	} while (result.first);

	return matches;
}

std::pair<bool, std::string> hgs::Client::SplitFirst(std::string& string, std::smatch& matcher, const std::regex& regex) const {

	bool result = std::regex_search(string, matcher, regex);
	return std::make_pair(result, matcher[0]);
}

void hgs::Client::End() {
	isOnline_ = false;
	attached_ = false;
}

void hgs::Client::DropLobbyConnections() {

	lobbyId = -1;
	lobbyMemory_ = nullptr;
}

bool hgs::Client::IsApiCall(std::string& string) {
	for (char character : string) {
		if (character == '#') return true;
	}
	return false;
}
void hgs::Client::PerformApiCall(std::string& call) {
	std::vector<std::string> segment = Split(call);

	// TODO add password support
	if (segment[0] == "#join" && segment.size() >= 2) {
		Lobby* target = nullptr;

		if (utilities::IsInt(segment[1])) {
			target = sharedMemory_->FindLobby(std::stoi(segment[1]));
		} else {
			target = sharedMemory_->FindLobby(segment[1]);
		}

		if (target == nullptr) {
			pendingSend_.append("{#|Lobby does not exist}");
			log_->warn("Client#" + std::to_string(id) + " tried to move to unknown lobby");
			return;
		} 
		if (target == lobbyMemory_->GetParent()) {
			pendingSend_.append("{#|Client is already located in targeted lobby}");
			return;
		}

		const std::string result = sharedMemory_->MoveClient(lobbyMemory_->GetParent(), target, this).second;
		pendingSend_.append("{#|" + result + "}");
	}
	else if (segment[0] == "#leave") {
		if (lobbyMemory_->GetParent() == sharedMemory_->GetMainLobby()) {
			pendingSend_.append("{#|Client is already located in main}");
			return;
		}
		const std::string result = sharedMemory_->MoveClient(lobbyMemory_->GetParent(), nullptr, this).second;
		pendingSend_.append(result);
	}
	else {
		log_->warn("Client command not performed");
	}
}

void hgs::Client::SetCoreCall(std::vector<int>& core_call) { coreCall_.push_back(core_call); }

void hgs::Client::SetSocket(const SOCKET socket) { socket_ = socket; }

void hgs::Client::SetId(const int id) { this->id = id; }

void hgs::Client::SetInterval(const std::chrono::microseconds microseconds) { loopInterval_ = microseconds; }

void hgs::Client::SetMemory(const gsl::not_null<SharedLobbyMemory*> lobby_memory) { lobbyMemory_ = lobby_memory; }

void hgs::Client::SetPause(const bool pause) { paused_ = pause; };

void hgs::Client::SetState(const State state) { state_ = state; }

void hgs::Client::SetPrevState(const State state) { lastState_ = state; };

void hgs::Client::SetOutgoing(std::vector<std::string>& outgoing) { outgoingCommands_ = outgoing; }
