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


Lobby::Lobby(const int id, std::string& name_tag, const int max_connections, SharedMemory* shared_memory) : maxConnections_(max_connections), sharedMemory_(shared_memory), id_(id), nameTag_(name_tag) {
	//lobbyState_ = none;
	internalState_ = none;
	coreCallPerformedCount_ = 0;
	connectedClients_ = 0;
	commandQueue_.clear();
	*lastCoreCall_ = {};

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

	// Generate a session log
	const std::string sessionLogPath = "sessions/";
	// If log directory does not exists it is created
	std::experimental::filesystem::create_directory(sessionLogPath);

	std::random_device rd;
	default_random_engine rand(rd());

	const std::string sessionIdentification = (!name_tag.empty() ? name_tag : std::to_string(id));
	sessionFile_ = '#' + sessionIdentification + '-' + std::to_string(abs(static_cast<int>(rand()))) + ".session.log";

	sessionLog_ = spdlog::basic_logger_mt(
		"SessionLog#" + sessionIdentification,
		"sessions/" + sessionFile_
	);
	sessionLog_->set_pattern("[%a %b %d %H:%M:%S %Y] %^ %v%$");
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

	std::ifstream file;
	file.open("sessions/" + sessionFile_);

	// Delete log file if empty
	const bool result = (file.peek() == std::ifstream::traits_type::eof());
	file.close();

	if (result) {
		//std::experimental::filesystem::remove("sessions/" + sessionFile_);
		log_->info("Deleted file, no content");
	}

	// Delete log
	spdlog::drop("Lobby#" + std::to_string(id_));
	spdlog::drop("SessionLog#" + (!nameTag_.empty() ? nameTag_ : std::to_string(id_)));
}

void Lobby::Execute() {

	while (running_) {
		Loop();
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

void Lobby::InitializeSending() const {

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

	sharedLobbyMemory_->SetState(none);
	log_->error("Timed out while waiting for client thread (Sending)");
	throw std::exception("Lobby could not give distribute information to all clients");
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
			if (current->GetState() == State::received) {
				current->SetState(State::done_receiving);
				std::string clientCommand = current->GetCommand();
				if (!clientCommand.empty()) {
					commandQueue_.push_back(current->GetCommand());

					sessionLog_->info("Client#" + std::to_string(current->id) + " " + clientCommand);
				} else {
					// Add empty command
					commandQueue_.push_back("{" + std::to_string(current->id) + "}");
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

	ResendReceive(connectedClients_ - readyClients);

	sharedLobbyMemory_->SetState(none);
	sessionLog_->flush();
}

void Lobby::ResendReceive(const int interrupted_connections) {

	log_->info("Resending data for clients");

	// Iterate through all clients
	Client* current = firstClient_;
	while (current != nullptr) {

		// Target clients which have not received anything
		if (current->GetState() == State::receiving) {
			current->SetCoreCall({0, lastCoreCall_[0], lastCoreCall_[1] , lastCoreCall_[2] });
			current->CoreCallListener();
			current->Send();
		}

		current = current->next;
	}

	// Ready clients excluding all clients that have already been processed
	int readyClients = 0;
	for (int i = 0; i < sharedMemory_->GetTimeoutTries(); i++) {
		// Iterate through all clients
		current = firstClient_;
		while (current != nullptr) {

			// Target clients which have not received anything
			if (current->GetState() == State::received) {
				current->SetState(State::done_receiving);
				std::string clientCommand = current->GetCommand();
				if (!clientCommand.empty()) {
					commandQueue_.push_back(current->GetCommand());
				}
				readyClients++;
			}

			current = current->next;
		}

		if (readyClients >= interrupted_connections) {
			return;
		}

		// Sleep for half a millisecond, convert milliseconds to microseconds
		std::this_thread::sleep_for(std::chrono::microseconds(static_cast<int>(sharedMemory_->GetTimeoutDelay() * 1000)));
	}
	
	// Kick non responding clients (all clients which are not ready at this point)

	current = (firstClient_ == nullptr ? nullptr : firstClient_);

	while (current != nullptr) {

		// Target clients which have not received anything
		if (current->GetState() != State::done_receiving) {
			auto* clientToDrop = current;
			current = current->next;
			log_->warn("Dropped client " + std::to_string(clientToDrop->id) + ". No response");
			DropClient(clientToDrop);
			continue;
		}

		current = current->next;
	}
}

void Lobby::ResetCommandQueue() {
	commandQueue_.clear();
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
				
				current->DropLobbyConnections();
				
				current->SetPrevState(none);
				current->SetState(none);
				// Tell client to drop all already existing externals
				current->SetOutgoing({ "{*|D}" });
				current->Send();
				return current;
			}

			current->End();
			return nullptr;
		}
		current = current->next;
	}
	log_->info("Client not found");
	return nullptr;
}

Client* Lobby::DropClient(Client* client, const bool detach_only) {
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

	connectedClients_--;
	log_->info("Dropped client " + std::to_string(client->id));

	if (detach_only) {
		// Tell other clients that this client has disconnected
		commandQueue_.push_back("{" + std::to_string(client->id) + "|D}");
		// Tell client to drop all already existing externals
		client->SetOutgoing({ "{*|D}" });
		client->SetPrevState(receiving);
		client->SetState(none);
		client->DropLobbyConnections();
		return client;
	}

	client->End();
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