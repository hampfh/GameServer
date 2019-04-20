#include "pch.h"
#include "Lobby.h"

SharedLobbyMemory::SharedLobbyMemory() {
	state_ = none;
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
	state_ = state;
}


Lobby::Lobby(const int id, const int max_connections, SharedMemory* shared_memory) : maxConnections_(max_connections), sharedMemory_(shared_memory), id(id) {
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
	log_ = std::make_shared<spdlog::logger>("Lobby#" + std::to_string(id), begin(sinks), end(sinks));
	log_->set_pattern("[%a %b %d %H:%M:%S %Y] [%L] %^%n: %v%$");
	spdlog::register_logger(log_);

	log_->info("Successfully created");
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
	spdlog::drop("Lobby#" + std::to_string(id));
}

void Lobby::Execute() {

	while (running_) {
		Loop();
	}
	CleanUp();

	delete this;
}

void Lobby::Loop() {

	DropAwaiting();

	// Receiving state
	if (internalState_ == State::receiving) {
		InitializeReceiving();
	}
	// Sending state
	else if (internalState_ == State::sending) {
		InitializeSending();
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
	std::this_thread::sleep_for(std::chrono::milliseconds(sharedMemory_->GetClockSpeed()));
}

void Lobby::InitializeSending() const {
	// Tell clients to start sending
	sharedLobbyMemory_->SetState(sending);

	int readyClients = 0;

	for (int i = 0; i < sharedMemory_->GetTimeoutTries(); i++) {
		// Iterate through all clients
		Client* current = firstClient_;
		while (current != nullptr) {

			if (current->GetState() == State::sending) {
				current->SetOutgoing(commandQueue_);
				current->SetState(State::awaiting);
				readyClients++;
			}

			current = current->next;
		}
		if (readyClients >= connectedClients_) {
			sharedLobbyMemory_->SetState(awaiting);
			return;
		}

		// Sleep for half a millisecond, convert milliseconds to microseconds
		std::this_thread::sleep_for(std::chrono::microseconds(static_cast<int>(sharedMemory_->GetTimeoutDelay() * 1000)));
	}

	sharedLobbyMemory_->SetState(awaiting);
	log_->warn("Timed out while waiting for client thread (Sending)");
}

void Lobby::InitializeReceiving() {
	// Tell clients to start receiving
	sharedLobbyMemory_->SetState(receiving);

	// Clear all earlier commands
	ResetCommandQueue();

	int readyClients = 0;

	for (int i = 0; i < sharedMemory_->GetTimeoutTries(); i++) {
		// Iterate through all clients
		Client* current = firstClient_;
		while (current != nullptr) {
			
			// If client has received response then take it
			if (current->GetState() == State::receiving) {
				current->SetState(State::awaiting);
				std::string clientCommand = current->GetCommand();
				if (!clientCommand.empty()) {
					commandQueue_.push_back(current->GetCommand());
				}
				readyClients++;
			}

			current = current->next;
		}
		if (readyClients >= connectedClients_) {
			sharedLobbyMemory_->SetState(awaiting);
			return;
		}
		if (i == 50) {
			log_->warn("Experiencing delays");
		}

		// Sleep for half a millisecond, convert milliseconds to microseconds
		std::this_thread::sleep_for(std::chrono::microseconds(static_cast<int>(sharedMemory_->GetTimeoutDelay() * 1000)));
	}

	// TODO kick the player which is lagging behind

	sharedLobbyMemory_->SetState(awaiting);
	log_->warn("Timed out while waiting for client thread (Receiving)");
	
}

void Lobby::ResetCommandQueue() {
	commandQueue_.clear();
}

void Lobby::BroadcastCoreCall(int& lobby, int& receiver, int& command) const {
	Client* current = firstClient_;

	while (current != nullptr) {
		current->SetCoreCall({0, lobby, receiver, command});
		current = current->next;
	}
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

int Lobby::AddClient(Client* client, const bool respect_limit) {

	if (connectedClients_ >= maxConnections_ && maxConnections_ != 0 && respect_limit) {
		log_->warn("Client could not be connected to lobby, lobby full");
		return 1;
	}

	// Give the client a pointer to the lobby memory
	client->SetMemory(sharedLobbyMemory_);
	client->lobbyId = this->id;

	connectedClients_++;

	// Connect to list
	if (firstClient_ == nullptr) {
		firstClient_ = client;
		lastClient_ = client;
	}
	else {
		Client* current = lastClient_;
		lastClient_->next = client;
		lastClient_ = lastClient_->next;
		lastClient_->prev = current;
	}
	log_->info("Client added");
	return 0;
}

Client* Lobby::DropClient(const int id, const bool detach_only) {
	Client* current = firstClient_;
	while (current != nullptr) {
		if (current->id == id) {
			// Drop client
			if (current == firstClient_ && firstClient_ == lastClient_) {
				firstClient_ = nullptr;
				lastClient_ = nullptr;
			}
			else if (current == firstClient_) {
				firstClient_ = current->next;
				firstClient_->prev = nullptr;
			} else if (current == lastClient_) {
				lastClient_ = current->prev;
				lastClient_->next = nullptr;
			} else {
				current->prev->next = current->next;
				current->next->prev = current->prev;
			}

			connectedClients_--;
			log_->info("Dropped client " + std::to_string(current->id));

			if (detach_only) {
				// Tell other clients that this client has disconnected
				commandQueue_.push_back("{" + std::to_string(current->id) +"|D}");
				// Tell client to drop all already existing externals
				current->SetOutgoing({"{*|D}"});
				current->SetPrevState(receiving);
				current->SetState(awaiting);
				current->DropLobbyConnections();
				return current;
			}

			// Delete client
			delete current;
			return nullptr;
		}
		current = current->next;
	}
	log_->info("Client not found");
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