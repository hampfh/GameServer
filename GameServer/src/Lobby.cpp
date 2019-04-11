#include "pch.h"
#include "Lobby.h"

Lobby::Lobby(const int id, SharedMemory* shared_memory) : sharedMemory_(shared_memory), id(id) {
	lobbyState_ = none;
	coreCallPerformedCount_ = 0;
	connectedClients_ = 0;
	maxConnections_ = 0;
	commandQueue_.clear();

	running_ = true;

	// Setup lobby logger
	std::vector<spdlog::sink_ptr> sinks;
	sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
	sinks.push_back(shared_memory->GetFileSink());
	log_ = std::make_shared<spdlog::logger>("Lobby#" + std::to_string(id), begin(sinks), end(sinks));
	log_->set_pattern("[%a %b %d %H:%M:%S %Y] [%L] %^%n: %v%$");
	spdlog::register_logger(log_);

	log_->info("Lobby#" + std::to_string(id) + " successfully created");
}


Lobby::~Lobby() {
	ResetCommandQueue();
}

void Lobby::Execute() {
	log_->info("Lobby#" + std::to_string(id) + " started successfully");

	while (running_) {
		Loop();
	}
	CleanUp();
}

void Lobby::Loop() {


	// Receiving state
	if (internalState_ == receiving) {
		InitializeReceiving();
	}
	// Sending state
	else if (internalState_ == sending) {
		InitializeSending();
	}


	// Swap state
	switch (internalState_) {
	case receiving:
		internalState_ = sending;
		break;
	case sending:
		internalState_ = receiving;
		break;
	default:
		internalState_ = receiving;
		break;
	}
	std::this_thread::sleep_for(std::chrono::milliseconds(sharedMemory_->GetClockSpeed()));
}

void Lobby::InitializeSending() {
	// Tell clients to start sending
	lobbyState_ = sending;

	int readyClients = 0;

	for (int i = 0; i < sharedMemory_->GetTimeoutTries(); i++) {
		// Iterate through all clients
		Client* current = firstClient_;
		while (current != nullptr) {

			if (current->GetState() == State::sending) {
				current->SetState(State::awaiting);
				readyClients++;
			}

			current = current->next;
		}
		if (readyClients >= connectedClients_) {
			lobbyState_ = awaiting;
			return;
		}

		// Sleep for half a millisecond, convert milliseconds to microseconds
		std::this_thread::sleep_for(std::chrono::microseconds(static_cast<int>(sharedMemory_->GetTimeoutDelay() * 1000)));
	}

	lobbyState_ = awaiting;
	log_->warn("Timed out while waiting for client thread (Sending)");
}

void Lobby::InitializeReceiving() {
	// Tell clients to start receiving
	lobbyState_ = receiving;

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
				commandQueue_.push_back(current->GetCommand());
				readyClients++;
			}

			current = current->next;
		}
		if (readyClients >= connectedClients_) {
			lobbyState_ = awaiting;
			return;
		}

		// Sleep for half a millisecond, convert milliseconds to microseconds
		std::this_thread::sleep_for(std::chrono::microseconds(static_cast<int>(sharedMemory_->GetTimeoutDelay() * 1000)));
	}

	lobbyState_ = awaiting;
	log_->warn("Timed out while waiting for client thread (Receiving)");
	
}

void Lobby::CleanUp() const {

}

//void Lobby::Ready() {
//	while (true) {
//		// If the clients state matches the servers then 
//		// consider the client ready for next state
//		if (clientStateMtx_.try_lock()) {
//			clientsReady_++;
//			clientStateMtx_.unlock();
//			return;
//		}
//		std::this_thread::sleep_for(std::chrono::microseconds(100));
//	}
//}
//
//void Lobby::AppendClientCommands(const int id, const std::string command) {
//	while (true) {
//		if (addCommandMtx_.try_lock()) {
//
//			// Add the clients coordinates to shared memory
//			commandQueue_.push_back(command);
//
//			// TODO Add a storage that saves all past commands to make it possible for clients to connect later
//
//			// Marks the clients as ready
//			addCommandMtx_.unlock();
//			return;
//		}
//		std::this_thread::sleep_for(std::chrono::microseconds(100));
//	}
//}
//
//void Lobby::AddCoreCall(const int receiver, const int command) {
//	// Create and append call
//	// Sender, Receiver, Command
//	coreCall_.push_back({ 0, receiver, command });
//}
//
//void Lobby::PerformedCoreCall() {
//	while (true) {
//		if (coreCallMtx_.try_lock()) {
//			coreCallPerformedCount_++;
//			if (coreCallPerformedCount_ == connectedClients_) {
//				ResetCoreCall();
//			}
//			coreCallMtx_.unlock();
//			return;
//		}
//		std::this_thread::sleep_for(std::chrono::microseconds(100));
//	}
//}
//
//void Lobby::ResetCoreCall() {
//	coreCallPerformedCount_ = 0;
//	coreCall_.clear();
//}

void Lobby::ResetCommandQueue() {
	commandQueue_.clear();
}

void Lobby::AddClient(Client* client) {
	log_->info("Client added");

	// TODO check lobby max number

	// Give the client a pointer to the lobby state
	client->SetLobbyStateReference(&lobbyState_);
	client->SetLobbyDropReference(&dropList_);

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
}

void Lobby::DropClient(const int id) {
	Client* current = firstClient_;
	while (true) {
		if (current->id != id) {
			// Drop client
			if (current == firstClient_) {
				current->next = firstClient_;
				firstClient_->prev = nullptr;
			} else if (current == lastClient_) {
				current->prev = lastClient_;
				lastClient_->next = nullptr;
			} else {
				current->prev->next = current->next;
			}

			connectedClients_--;
			// Delete client
			delete current;
			break;
		}
		current = current->next;
	}
}

void Lobby::DropAwaiting() {
	for (auto clientId : dropList_) {
		DropClient(clientId);
		log_->info("Dropped client " + std::to_string(id));
	}
}