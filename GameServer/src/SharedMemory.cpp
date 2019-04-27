#include "pch.h"
#include "SharedMemory.h"

SharedMemory::SharedMemory() {

	clockSpeed_ = 0;

	// Create a global file sink
	SetupLogging();

	coreCall_.clear();
}

SharedMemory::~SharedMemory() {
	// Delete all lobbies
	Lobby* current = firstLobby_;
	Lobby* prev = firstLobby_;
	// Stop threads
	while(current != nullptr) {
		current = current->next;
		prev->Drop();
		prev = current;
	}
	std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

void SharedMemory::SetupLogging() {
	// Global spdlog settings
	spdlog::flush_every(std::chrono::seconds(10));
	spdlog::set_pattern("[%a %b %d %H:%M:%S %Y] [%L] %^%n: %v%$");

	// Create global sharedFileSink
	sharedFileSink_ = std::make_shared<spdlog::sinks::rotating_file_sink_mt>("logs/log.log", 1048576 * 5, 3);

	// Setup memory logger
	std::vector<spdlog::sink_ptr> sinks;
	sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
	sinks.push_back(sharedFileSink_);
	log_ = std::make_shared<spdlog::logger>("Shared Memory", begin(sinks), end(sinks));
	log_->set_pattern("[%a %b %d %H:%M:%S %Y] [%L] %^%n: %v%$");
	register_logger(log_);
}

void SharedMemory::AddSocket(const SOCKET new_socket) {
	while (true) {
		if (addSocketMtx_.try_lock()) {
			// Add newClient to the socketList
			FD_SET(new_socket, &sockets_);
			// Increase connected clients number
			connectedClients_++;
			addSocketMtx_.unlock();
			return;
		}
		std::this_thread::sleep_for(std::chrono::microseconds(100));
	}
}

void SharedMemory::DropSocket(const SOCKET socket) {
	while (true) {
		if (dropSocketMtx_.try_lock()) {
			// Decrease online clients
			connectedClients_--;
			// Clear the socket
			closesocket(socket);
			// Remove socket from socketList
			FD_CLR(socket, &sockets_);
			dropSocketMtx_.unlock();
			return;
		}
		std::this_thread::sleep_for(std::chrono::microseconds(100));
	}
}

Client* SharedMemory::FindClient(const int client_id, Lobby** lobby) const {
	Lobby* current = firstLobby_;

	while (current != nullptr) {
		Client* client = current->FindClient(client_id);
		if (client != nullptr) {
			*lobby = current;
			return client;
		}
		current = current->next;
	}
	*lobby = nullptr;
	return nullptr;
}

Lobby* SharedMemory::FindLobby(const int lobby_id) const {
	Lobby* current = firstLobby_;

	while (current != nullptr) {
		if (current->GetId() == lobby_id) {
			return current;
		}
		current = current->next;
	}
	return nullptr;
}

Lobby* SharedMemory::FindLobby(std::string& name_tag) const {
	Lobby* current = firstLobby_;

	while (current != nullptr) {
		if (current->GetNameTag() == name_tag) {
			return current;
		}
		current = current->next;
	}
	return nullptr;
}

Lobby* SharedMemory::AddLobby(std::string name) {
	while (true) {
		if (addLobbyMtx_.try_lock()) {
			if (lobbyMax_ <= lobbiesAlive_ && lobbyMax_ != 0) {
				log_->warn("Lobby max reached");
				return nullptr;
			}

			// Add a new lobby and assign an id_
			Lobby* newLobby = new Lobby(lobbyIndex_++, name, lobbyMax_, this);

			// Connect to list
			if (firstLobby_ == nullptr) {
				// The first lobby is always "main"
				mainLobby_ = newLobby;
				firstLobby_ = newLobby;
				lastLobby_ = newLobby;
			}
			else {
				Lobby* current = lastLobby_;
				lastLobby_->next = newLobby;
				lastLobby_ = lastLobby_->next;
				lastLobby_->prev = current;
			}

			// Release lobby to another thread
			// Connect the new client to a new thread
			std::thread lobbyThread(&Lobby::Execute, newLobby);
			lobbyThread.detach();

			addLobbyMtx_.unlock();
			return newLobby;
		}
		std::this_thread::sleep_for(std::chrono::microseconds(100));
	}
}

Lobby* SharedMemory::CreateMainLobby() {
	while (true) {
		if (addLobbyMtx_.try_lock()) {
			if (mainLobby_ != nullptr) {
				log_->info("Tried to create secondary main lobby, ignoring");
				return nullptr;
			}
			std::string name = "main";
			// Add a new lobby and assign an id_
			Lobby* newLobby = new Lobby(lobbyIndex_++, name, lobbyMax_, this);

			lobbiesAlive_++;

			mainLobby_ = newLobby;
			firstLobby_ = newLobby;
			lastLobby_ = newLobby;

			// Release lobby to another thread
			// Connect the new client to a new thread
			std::thread lobbyThread(&Lobby::Execute, newLobby);
			lobbyThread.detach();

			addLobbyMtx_.unlock();
			return newLobby;
		}

		std::this_thread::sleep_for(std::chrono::microseconds(100));
	}
}

void SharedMemory::DropLobby(const int id) {
	while (true) {
		if (dropLobbyMtx_.try_lock()) {
			// Delete lobby, (disconnect all clients)

			Lobby* current = firstLobby_;

			// Find lobby in list
			while (current != nullptr) {
				// Disconnect lobby from list
				if (current->GetId() == id) {
					// Targeted lobby is first in list
					if (current == firstLobby_ && firstLobby_ == lastLobby_) {
						firstLobby_ = nullptr;
						lastLobby_ = nullptr;
					}
					else if (current == firstLobby_) {
						firstLobby_ = current->next;
						firstLobby_->prev = nullptr;
					}
						// Targeted lobby is last in list
					else if (current == lastLobby_) {
						lastLobby_ = current->prev;
						lastLobby_->next = nullptr;
					}
						// Target lobby is somewhere in middle of list
					else { current->prev->next = current->next; }

					lobbiesAlive_--;

					// Delete the lobby
					log_->info("Dropped lobby " + std::to_string(current->GetId()));
					current->Drop();
					break;
				}

				current = current->next;
			}

			dropLobbyMtx_.unlock();
			return;
		}
		std::this_thread::sleep_for(std::chrono::microseconds(100));
	}
}

bool SharedMemory::IsInt(std::string& string) const {	
	try {
		// TODO iterate through string and do not rely on error
		std::stoi(string);
		return true;
	}
	catch (...) { return false; }
}

void SharedMemory::AddCoreCall(const int lobby, const int receiver, const int command) {
	// Create and append call
	// Sender, Receiver, Command
	coreCall_.push_back({0, lobby, receiver, command});
}

Lobby* SharedMemory::GetLobby(const int id) const {
	Lobby* current = firstLobby_;

	// Find lobby in list
	while (current != nullptr) {
		// Disconnect lobby from list
		if (current->GetId() == id) { return current; }

		current = current->next;
	}
	return nullptr;
}

int SharedMemory::GetLobbyId(std::string& string) const {
	if (!IsInt(string)) {
		Lobby* targetLobby = FindLobby(string);
		if (targetLobby != nullptr) {
			return targetLobby->GetId();
		}
		
		log_->warn("No such lobby found");
		return -1;
	}	
	return std::stoi(string);
}

void SharedMemory::SetConnectedClients(const int connected_clients) { connectedClients_ = connected_clients; }

void SharedMemory::SetSockets(const fd_set list) { sockets_ = list; }

void SharedMemory::SetTimeoutTries(const int tries) { timeoutTries_ = tries; }

void SharedMemory::SetTimeoutDelay(const float delay) { timeoutDelay_ = delay; }

void SharedMemory::SetClockSpeed(const int clock_speed) { clockSpeed_ = clock_speed; }

void SharedMemory::SetLobbyMax(const int lobby_max) { lobbyMax_ = lobby_max; };

void SharedMemory::SetLobbyStartId(const int start_id) { lobbyIndex_ = start_id; };