#pragma once
#include "lobby.h"
#include "utilities.h"

namespace hgs {
	class MasterLobby;

	class MasterLobbyUpStream {
		std::deque<std::tuple<Client*, Lobby*, Lobby*>> moveClientProcessingQueue_;

	public:
		void AddClientMove(gsl::not_null<Client*> client, gsl::not_null<Lobby*> source, Lobby* target);

		std::deque<std::tuple<Client*, Lobby*, Lobby*>> GetClientProcessingQueue() const { return moveClientProcessingQueue_; };

		void ClearClientProcessingQueue();
	};

	class MasterLobby : Lobby {
		std::deque<Lobby*> lobbyList_;

		std::mutex dropSocketMtx_;
		std::mutex addLobbyMtx_;
		std::mutex dropLobbyMtx_;

		int lobbiesAlive_ = 0;
		int lobbyIndex_ = 0;

		MasterLobbyUpStream* upstream_;

		std::shared_ptr<spdlog::sinks::rotating_file_sink<std::mutex>> fileSink_;

	public:

		MasterLobby(int id, std::string& name_tag, Configuration* conf, std::shared_ptr<spdlog::sinks::rotating_file_sink<std::mutex>>& file_sink);
		~MasterLobby();
		Lobby* AddLobby(std::string& name);
		void DropLobby(int id);
		void DropLobby(Lobby* lobby);
		void MoveQueuedClients();
	};
}

