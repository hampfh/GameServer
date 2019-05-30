#include "pch.h"
#include "master_lobby.h"

void hgs::MasterLobbyUpStream::AddClientMove(gsl::not_null<Client*> client, gsl::not_null<Lobby*> source, Lobby* target) {
	moveClientProcessingQueue_.emplace_back(std::make_tuple(client, source, target));
}

void hgs::MasterLobbyUpStream::ClearClientProcessingQueue() {
	moveClientProcessingQueue_.clear();
}

hgs::MasterLobby::MasterLobby(const int id, std::string& name_tag, Configuration* conf, std::shared_ptr<spdlog::sinks::rotating_file_sink<std::mutex>>& file_sink) : 
Lobby(id, name_tag, conf, file_sink), fileSink_(file_sink) {
	lobbyIndex_ = conf_->lobbyStartIdAt;

}

hgs::MasterLobby::~MasterLobby() {

	for (auto lobby : lobbyList_) {
		lobby->Drop();
	}
	std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

hgs::Lobby* hgs::MasterLobby::AddLobby(std::string& name) {
	while (true) {
		if (addLobbyMtx_.try_lock()) {
			if (conf_->lobbyMaxConnections <= lobbiesAlive_ && conf_->lobbyMaxConnections != 0) {
				log_->warn("Lobby max reached");
				addLobbyMtx_.unlock();
				return nullptr;
			}

			// Add a new lobby and assign an id_
			Lobby* newLobby = new Lobby(lobbyIndex_++, name, conf_, fileSink_);
			lobbiesAlive_++;

			lobbyList_.push_back(newLobby);

			// Redirect lobby to another thread
			// Connect the new client to a new thread
			std::thread lobbyThread(&Lobby::Execute, newLobby);
			lobbyThread.detach();

			addLobbyMtx_.unlock();
			return newLobby;
		}
		std::this_thread::sleep_for(std::chrono::microseconds(100));
	}
}

void hgs::MasterLobby::DropLobby(const int id) {
	while (true) {
		if (dropLobbyMtx_.try_lock()) {
			// Delete lobby, (disconnect all clients)

			for (auto lobby : lobbyList_) {
				if (lobby->GetId() == id) {
					DropLobby(lobby);
					break;
				}
			}

			dropLobbyMtx_.unlock();
			return;
		}
		std::this_thread::sleep_for(std::chrono::microseconds(100));
	}
}

void hgs::MasterLobby::DropLobby(Lobby* lobby) {

	// Remove first element that match in the list
	lobbyList_.erase(
		std::remove(
			lobbyList_.begin(), 
			lobbyList_.end(), 
			lobby
		), 
		lobbyList_.end()
	);

	// Delete the lobby
	lobby->Drop();
	lobbiesAlive_--;
	log_->info("Dropped lobby " + std::to_string(lobby->GetId()));
}

void hgs::MasterLobby::MoveQueuedClients() {
	for (auto object : upstream_->GetClientProcessingQueue()) {
		Client* client = std::get<0>(object);
		Lobby* source = std::get<1>(object);
		Lobby* target = std::get<2>(object);

		source->DropClient(client, true, true);

		if (target == nullptr) {
			this->AddClient(client, true, false);
		} else {
			target->AddClient(client, true, true);
		}
	}
	upstream_->ClearClientProcessingQueue();
}