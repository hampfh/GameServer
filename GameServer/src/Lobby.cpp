#include "pch.h"
#include "Lobby.h"

SharedLobbyMemory::SharedLobbyMemory() {
	state_ = none;
	pauseState_ = 0;
	nextState_ = none;
}

void SharedLobbyMemory::AddDrop(const int id) {
	while(true) {
		if (addDropMtx_.try_lock()) {
			dropList_.push_back(id);
			addDropMtx_.unlock();
			return;
		}
		std::this_thread::sleep_for(std::chrono::microseconds(500));
	}
}

void SharedLobbyMemory::ClearDropList() {
	while (true) {
		if (addDropMtx_.try_lock()) {
			dropList_.clear();
			addDropMtx_.unlock();
			return;
		}
		std::this_thread::sleep_for(std::chrono::microseconds(500));
	}
}

void SharedLobbyMemory::SetState(const State state) {
	switch (state) {
	case State::receiving:
		nextState_ = State::sending;
	case State::sending:
		nextState_ = State::receiving;
	default:
		break;
	}
		
	state_ = state;
}

void SharedLobbyMemory::SetPauseState(const int pre_pause) {
	while (true) {
		if (setPauseMtx_.try_lock()) {
			pauseState_ = pre_pause;
			setPauseMtx_.unlock();
			return;
		}
	}
}

Lobby::Lobby(const int id, std::string& name_tag, const int max_connections, SharedMemory* shared_memory) : maxConnections_(max_connections), sharedMemory_(shared_memory), id_(id), nameTag_(name_tag) {
	//lobbyState_ = none;
	internalState_ = none;
	coreCallPerformedCount_ = 0;
	connectedClients_ = 0;
	commandQueue_.clear();

	running_ = true;
	sharedLobbyMemory_ = new SharedLobbyMemory;

	// Setup lobby logger
	std::vector<spdlog::sink_ptr> sinks;
	sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
	sinks.push_back(shared_memory->GetFileSink());
	log_ = std::make_shared<spdlog::logger>("Lobby#" + (!name_tag.empty() ? name_tag : std::to_string(id)), begin(sinks), end(sinks));
	log_->set_pattern("[%a %b %d %H:%M:%S %Y] [%L] %^%n: %v%$");
	spdlog::register_logger(log_);

	log_->info("Successfully created lobby [#" + std::to_string(id) + (!name_tag.empty() && name_tag.length() > 0 ? "] [#" + name_tag : "") + "]");

	if (sharedMemory_->GetSessionLogging()) {
		// Generate a session log
		const std::string sessionLogPath = "sessions/";
		// If log directory does not exists it is created
		std::experimental::filesystem::create_directory(sessionLogPath);

		std::random_device rd;
		default_random_engine rand(rd());

		const std::string sessionIdentification = (!name_tag.empty() ? name_tag : std::to_string(id));
		sessionFile_ = "sessions/#" + sessionIdentification + '-' + std::to_string(abs(static_cast<int>(rand()))) + ".session.log";

		sessionLog_ = spdlog::basic_logger_mt(
			"SessionLog#" + sessionIdentification,
			sessionFile_
		);
		sessionLog_->set_pattern("[%a %b %d %H:%M:%S %Y] %^ %v%$");
	} else {
		sessionLog_ = nullptr;
	}
}


Lobby::~Lobby() {
}

void Lobby::CleanUp() {
	// Delete all client
	Client* current = firstClient_;
	Client* prev = firstClient_;
	while (current != nullptr) {
		sharedMemory_->DropSocket(current->GetSocket());
		log_->info("Dropped client #" + std::to_string(current->id));
		
		current = current->next;
		prev->End();
		prev = current;
	}
	DropAwaiting();

	// Delete log
	spdlog::drop("Lobby#" + std::to_string(id_));
	spdlog::drop("SessionLog#" + (!nameTag_.empty() ? nameTag_ : std::to_string(id_)));
}

void Lobby::Execute() {

	while (running_) {

		switch (sharedLobbyMemory_->GetPauseState()) {
		case 0:
			Loop();
			break;
		case 1:
			// Confirm pause
			sharedLobbyMemory_->SetPauseState(2);
			break;
		default:
			break;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(sharedMemory_->GetClockSpeed()));
	}
	CleanUp();

	delete this;
}

void Lobby::Loop() {

	DropAwaiting();

	if (connectedClients_ > 0) {

		// Receiving state
		if (internalState_ == State::receiving) {
			InitializeReceiving();
		}
		// Sending state
		else if (internalState_ == State::sending) {
			InitializeSending();
		}
	}

	// Swap state
	switch (internalState_) {
	case State::receiving:
		internalState_ = State::sending;
		break;
	case sending:
		internalState_ = State::receiving;
		break;
	default:
		internalState_ = State::receiving;
		break;
	}
}

void Lobby::InitializeSending() {

	// Iterate through all clients
	Client* current = firstClient_;
	while (current != nullptr) {

		// Give client data
		current->SetOutgoing(commandQueue_);

		current = current->next;
	}

	// Tell clients to start sending
	sharedLobbyMemory_->SetState(sending);

	int readyClients = 0;

	// Check if all clients have sent
	for (int i = 0; i < sharedMemory_->GetTimeoutTries(); i++) {
		// Iterate through all clients
		current = firstClient_;
		while (current != nullptr) {

			if (current->GetState() == State::sent) {
				current->SetState(State::done_sending);
				readyClients++;
			}

			current = current->next;
		}
		if (readyClients >= connectedClients_) {
			sharedLobbyMemory_->SetState(none);
			return;
		}

		// Sleep for half a millisecond, convert milliseconds to microseconds
		std::this_thread::sleep_for(std::chrono::microseconds(static_cast<int>(sharedMemory_->GetTimeoutDelay() * 1000)));
	}

	DropNonResponding(State::done_sending);

	sharedLobbyMemory_->SetState(none);
}

void Lobby::InitializeReceiving() {
	// Tell clients to start receiving
	sharedLobbyMemory_->SetState(receiving);

	// Clear all earlier commands
	commandQueue_.clear();

	int readyClients = 0;

	for (int i = 0; i < sharedMemory_->GetTimeoutTries(); i++) {
		// Iterate through all clients
		Client* current = firstClient_;
		while (current != nullptr) {
			
			// If client has received response then take it
			if (current->GetState() == State::received) {
				current->SetState(State::done_receiving);
				std::string clientCommand = current->GetCommand();
				if (!clientCommand.empty()) {
					commandQueue_.push_back(current->GetCommand());

					// Create log if enabled
					if (sessionLog_ != nullptr) {
						sessionLog_->info("Client#" + std::to_string(current->id) + " " + clientCommand);
					}
				} 
				readyClients++;
			}

			current = current->next;
		}
		if (readyClients >= connectedClients_) {
			sharedLobbyMemory_->SetState(none);
			return;
		}

		// Sleep for half a millisecond, convert milliseconds to microseconds
		std::this_thread::sleep_for(std::chrono::microseconds(static_cast<int>(sharedMemory_->GetTimeoutDelay() * 1000)));
	}

	DropNonResponding(State::done_receiving);

	sharedLobbyMemory_->SetState(none);

	// Flush the session log 
	if (sharedMemory_->GetSessionLogging()) {
		sessionLog_->flush();
	}
}

void Lobby::Resend(const State resend_on) {
	// Iterate through all clients
	Client* current = firstClient_;
	while (current != nullptr) {

		// Target clients which have not received anything
		if (current->GetState() == resend_on) {
			// Assign the latest core call to the client
			current->SetCoreCall({ 0, lastCoreCall_[0], lastCoreCall_[1] , lastCoreCall_[2] });
			current->SetOutgoing(commandQueue_);
			current->CoreCallListener();
			current->Send();
		}

		current = current->next;
	}
}

void Lobby::DropNonResponding(const State non_condition_state) {
	// Kick non responding clients (all clients which are not ready at this point)
	Client* current = (firstClient_ == nullptr ? nullptr : firstClient_);

	while (current != nullptr) {

		// Target clients which have not received anything
		if (current->GetState() != non_condition_state) {
			auto* clientToDrop = current;
			current = current->next;
			log_->warn("Dropped client " + std::to_string(clientToDrop->id) + ". No response");
			DropClient(clientToDrop);
			continue;
		}

		current = current->next;
	}
}

void Lobby::BroadcastCoreCall(int& lobby, int& receiver, int& command) {
	Client* current = firstClient_;

	// Update last core call
	lastCoreCall_[0] = lobby;
	lastCoreCall_[1] = receiver;
	lastCoreCall_[2] = command;

	while (current != nullptr) {
		current->SetCoreCall({0, lobby, receiver, command});
		current = current->next;
	}
}

void Lobby::WaitForPause(const State perform_on_state) const {
	while (true) {
		// Wait until lobby has the correct state to continue
		if (sharedLobbyMemory_->GetNextState() == perform_on_state) { // TODO THIS FREEZES THE PROGRAM
			WaitForPause();
			return;
		}
		std::this_thread::sleep_for(std::chrono::microseconds(50));
	}
}

void Lobby::WaitForPause() const {
	// Send pause request to lobby loop
	sharedLobbyMemory_->SetPauseState(1);

	// Wait for lobby to start pause state
	while (sharedLobbyMemory_->GetPauseState() != 2) {
		std::this_thread::sleep_for(std::chrono::microseconds(500));
	}
}

std::string Lobby::List() const {
	
	std::string result = "\n======= Lobby list ========\n";
	result.append("[" + std::to_string(connectedClients_) + "] connected clients");
	if (connectedClients_ > 0) {
		result.append("\n---------------------------");
	}

	// Composer list of clients
	Client* current = firstClient_;
	while (current != nullptr) {
		result.append("\nClient#" + std::to_string(current->id));
		current = current->next;
	}
	
	result.append("\n===========================");

	// Print data
	log_->info(result);
	return result;
}

Client* Lobby::FindClient(const int id) const {
	Client* current = firstClient_;

	while(current != nullptr) {
		if (current->id == id) {
			return current;
		}
		current = current->next;
	}
	return nullptr;
}

int Lobby::AddClient(Client* client, const bool respect_limit, const bool external) {

	// Make sure max limit for lobby has not been reached
	if (connectedClients_ >= maxConnections_ && maxConnections_ != 0 && respect_limit) {
		log_->warn("Client could not be connected to lobby, lobby full");
		return 1;
	}

	// Pause the lobby
	WaitForPause();

	// Give the client a pointer to the lobby memory
	client->SetMemory(sharedLobbyMemory_);
	client->lobbyId = this->id_;
	client->SetState(none);
	client->SetPrevState((this->internalState_ == State::sending ? State::received : State::sending));

	connectedClients_++;

	// Connect to list
	if (firstClient_ == nullptr) {
		firstClient_ = client;
		lastClient_ = client;
	}
	else {
		Client* prevLast = lastClient_;
		prevLast->next = client;
		lastClient_ = client;
		client->prev = prevLast;
	}
	log_->info("Added client");

	// Continue lobby loop
	sharedLobbyMemory_->SetPauseState(0);

	return 0;
}

Client* Lobby::DropClient(const int id, const bool detach_only, const bool external) {
	Client* current = firstClient_;
	// Search for client
	while (current != nullptr) {
		if (current->id == id) {
			// Call for a drop
			return DropClient(current, detach_only, external);
		}
		current = current->next;
	}
	log_->info("Client not found");
	return nullptr;
}

Client* Lobby::DropClient(Client* client, const bool detach_only, const bool external) {

	if (client == nullptr) return nullptr;

	if (external) {
		// Pause lobby
		WaitForPause(); // TODO only on send
	}

	// Drop client
	if (client == firstClient_ && firstClient_ == lastClient_) {
		firstClient_ = nullptr;
		lastClient_ = nullptr;
	}
	else if (client == firstClient_) {
		firstClient_ = client->next;
		firstClient_->prev = nullptr;
	}
	else if (client == lastClient_) {
		lastClient_ = client->prev;
		lastClient_->next = nullptr;
	}
	else {
		client->prev->next = client->next;
		client->next->prev = client->prev;
	}

	// Detach clients connections 
	client->next = nullptr;
	client->prev = nullptr;

	connectedClients_--;
	log_->info("Dropped client " + std::to_string(client->id));

	// Tell other clients that this client has disconnected
	commandQueue_.push_back("{" + std::to_string(client->id) + "|D}");

	if (detach_only) {

		client->DropLobbyConnections();

		client->SetPrevState(none);
		client->SetState(none);
		// Tell client to drop all already existing externals
		client->SetOutgoing({ "{*|D}" });
		client->Send();

		// Continue lobby loop
		sharedLobbyMemory_->SetPauseState(0);

		return client;
	}

	client->End();

	// Continue lobby loop
	sharedLobbyMemory_->SetPauseState(0);

	return nullptr;
}

void Lobby::DropAwaiting() {
	for (auto clientId : sharedLobbyMemory_->GetDropList()) {
		
		DropClient(clientId);
	}
	sharedLobbyMemory_->ClearDropList();
}

void Lobby::Drop() {
	running_ = false;
}