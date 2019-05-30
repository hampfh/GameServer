#include "pch.h"
#include "shared_memory.h"
#include "utilities.h"

hgs::SharedMemory::SharedMemory(const Configuration& conf) : conf_(conf) {

	// Create a global file sink
	SetupLogging();

	coreCall_.clear();
}

hgs::SharedMemory::~SharedMemory() {
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

void hgs::SharedMemory::SetupLogging() {
	const std::string logFilePath = conf_.logPath;

	// If log directory does not exists it is created
	std::experimental::filesystem::create_directory(logFilePath);

	// Global spdlog settings
	spdlog::flush_every(std::chrono::seconds(4));
	spdlog::flush_on(spdlog::level::warn);
	spdlog::set_pattern("[%a %b %d %H:%M:%S %Y] [%L] %^%n: %v%$");
	
	// Create global sharedFileSink
	try {
		
		sharedFileSink_ = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(logFilePath + "log.log", 1048576 * 5, 3);

		// Setup memory logger
		std::vector<spdlog::sink_ptr> sinks;
		sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
		sinks.push_back(sharedFileSink_);
		log_ = std::make_shared<spdlog::logger>("Shared Memory", begin(sinks), end(sinks));
		log_->set_pattern("[%a %b %d %H:%M:%S %Y] [%L] %^%n: %v%$");
		register_logger(log_);
	} catch (spdlog::spdlog_ex) {
		std::cout << "Error, could not create logger. Is the logPath set correctly?" << std::endl;
		throw std::exception();
	}
}

void hgs::SharedMemory::AddSocket(const SOCKET new_socket) {
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

void hgs::SharedMemory::DropSocket(const SOCKET socket) {
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

std::pair<int, std::string> hgs::SharedMemory::MoveClient(gsl::not_null<Lobby*> source, Lobby* target, gsl::not_null<Client*> client) const {

	// Remove client from current lobby
	source->DropClient(client, true, false);

	if (target == nullptr) {
		target = mainLobby_;
	}
	// Add client to the selected lobby
	if (target->AddClient(client, true, false) != 0) {
		// Could not add client

		// Kick client
		client->End();
		//TODO error checking
		return std::make_pair(1, "Client was dropped, access denied");
	}

	return std::make_pair(0, "Success");
}

std::pair<hgs::Client*, hgs::Lobby*> hgs::SharedMemory::FindClient(const int client_id) const {
	Lobby* current = firstLobby_;

	while (current != nullptr) {
		Client* client = current->FindClient(client_id);
		if (client != nullptr) {
			return std::make_pair(client, current);
		}
		current = current->next;
	}
	return std::make_pair(nullptr, nullptr);
}

hgs::Lobby* hgs::SharedMemory::FindLobby(const int lobby_id) const {
	Lobby* current = firstLobby_;

	while (current != nullptr) {
		if (current->GetId() == lobby_id) {
			return current;
		}
		current = current->next;
	}
	return nullptr;
}

hgs::Lobby* hgs::SharedMemory::FindLobby(std::string& name_tag) const {
	Lobby* current = firstLobby_;

	while (current != nullptr) {
		if (current->GetNameTag() == name_tag) {
			return current;
		}
		current = current->next;
	}
	return nullptr;
}

hgs::Lobby* hgs::SharedMemory::AddLobby(std::string name) {
	while (true) {
		if (addLobbyMtx_.try_lock()) {
			if (conf_.lobbyMaxConnections <= lobbiesAlive_ && conf_.lobbyMaxConnections != 0) {
				log_->warn("Lobby max reached");
				addLobbyMtx_.unlock();
				return nullptr;
			}

			// Add a new lobby and assign an id_
			Lobby* newLobby = new Lobby(lobbyIndex_++, name, this, &conf_);
			lobbiesAlive_++;

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

hgs::Lobby* hgs::SharedMemory::CreateMainLobby() {
	while (true) {
		if (addLobbyMtx_.try_lock()) {
			if (mainLobby_ != nullptr) {
				log_->info("Tried to create secondary main lobby, ignoring");
				return nullptr;
			}
			std::string name = "main";
			// Add a new lobby and assign an id_
			Lobby* newLobby = new Lobby(lobbyIndex_++, name, this, &conf_);

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

void hgs::SharedMemory::DropLobby(const int id) {
	while (true) {
		if (dropLobbyMtx_.try_lock()) {
			// Delete lobby, (disconnect all clients)

			Lobby* current = firstLobby_;

			// Find lobby in list
			while (current != nullptr) {
				// Disconnect lobby from list
				if (current->GetId() == id) {
					DropLobby(current);
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

void hgs::SharedMemory::DropLobby(Lobby* lobby) {
	// Targeted lobby is first in list
	if (lobby == firstLobby_ && firstLobby_ == lastLobby_) {
		firstLobby_ = nullptr;
		lastLobby_ = nullptr;
	}
	else if (lobby == firstLobby_) {
		firstLobby_ = lobby->next;
		firstLobby_->prev = nullptr;
	}
	// Targeted lobby is last in list
	else if (lobby == lastLobby_) {
		lastLobby_ = lobby->prev;
		lastLobby_->next = nullptr;
	}
	// Target lobby is somewhere in middle of list
	else { 
		lobby->prev->next = lobby->next; 
		lobby->next->prev = lobby->prev;
	}

	// Delete the lobby
	lobby->Drop();
	lobbiesAlive_--;
	log_->info("Dropped lobby " + std::to_string(lobby->GetId()));
}

void hgs::SharedMemory::AddCoreCall(const int lobby, const int receiver, const int command) {
	// Create and append call
	// Sender, Receiver, Command
	coreCall_.push_back({0, lobby, receiver, command});
}

int hgs::SharedMemory::GetLobbyId(std::string& string) const {
	Lobby* target;

	if (utilities::IsInt(string)) {
		target = FindLobby(std::stoi(string));
		if (target != nullptr) {
			return target->GetId();
		}
	}
	else {
		target = FindLobby(string);
		if (target != nullptr) {
			return target->GetId();
		}
	}

	return -1;
}

void hgs::SharedMemory::SetSockets(const fd_set list) { sockets_ = list; }