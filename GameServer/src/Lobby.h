#pragma once
#include "SharedMemory.h"
#include "client.h"
#include "utilities.h"

/**
	Lobby.h
	Purpose: The lobby is a way of grouping clients to each other. 
	This enables the server to have multiple clients running in different sections without interrupting each other

	@author Hampus Hallkvist
	@version 0.3 07/05/2019
*/

namespace hgs {

	// Predefining classes
	class SharedMemory;
	class Client;

	class SharedLobbyMemory {
	public:
		SharedLobbyMemory(int id);
		/**
			Add a client which the
			lobby will remove next iteration

			@parma id_ Id of client
			@return void
		 */
		void AddDrop(int id);
		/**
			Clear drop list from all ids

			@return void
		 */
		void ClearDropList();
		void AddClientMove(gsl::not_null<Client*> client, Lobby* target);
		void ClearMoveList();

		// Getters

		State GetState() const { return state_; };
		State GetNextState() const { return nextState_; };
		State* GetStatePointer() { return &state_; };
		std::vector<int> GetDropList() const { return dropList_; };
		std::deque<std::pair<Client*, Lobby*>> GetMoveList() const { return moveList_; };
		int GetPauseState() const { return pauseState_; };
		int GetId() const { return id_; };

		// Setters

		void SetState(State state);
		void SetPauseState(int pre_pause);
	private:
		State state_;
		State nextState_;
		int pauseState_;
		std::vector<int> dropList_;
		std::deque<std::pair<Client*, Lobby*>> moveList_;
		const int id_;

		std::mutex addDropMtx_;
		std::mutex addMoveMtx_;
		std::mutex setPauseMtx_;
	};

	class Lobby {
	public:
		Lobby(int id, std::string& name_tag, Configuration* conf, std::shared_ptr<spdlog::sinks::rotating_file_sink<std::mutex>>& file_sink);

		void CleanUp();
		void Execute();
		void Loop();
		void InitializeSending();
		void InitializeReceiving();
		void BroadcastCoreCall(int& lobby, int& receiver, int& command);
		void WaitForPause(State perform_on_state) const;
		void WaitForPause() const;
		std::string List() const;
		Client* FindClient(int id) const;
		int AddClient(Client* client, bool respect_limit = true, bool external = false);
		void DropNonResponding(State non_condition_state);
		void DropAwaiting();
		void MoveAwaiting();
		Client* DropClient(int id, bool detach_only = false, bool external = false);
		Client* DropClient(Client* client, bool detach_only = false, bool external = false);
		int DropAll();
		// Terminates loop
		void Drop();

		// Getters
		std::vector<std::string> GetClientCommands() const { return commandQueue_; };
		int GetConnectedClients() const { return connectedClients_; };
		int GetId() const { return id_; };
		std::string GetNameTag() const { return nameTag_; };
	protected:
		bool running_;

		State internalState_ = none;

		Configuration* conf_;

		// All clients connected to the lobby
		int connectedClients_;

		// Points to next lobby
		Client* firstClient_ = nullptr;
		Client* lastClient_ = nullptr;

		// Shared pointer to logger
		std::shared_ptr<spdlog::logger> log_;
		// A session log logs all data the clients send between each other
		std::shared_ptr<spdlog::logger> sessionLog_;

		// Mutex objects used to for thread safety 

		std::mutex clientStateMtx_;
		std::mutex addCommandMtx_;
		std::mutex coreCallMtx_;
		std::mutex setPauseMtx_;

		// Dynamic allocated array holding all clients responses
		std::vector<std::string> commandQueue_;

		int lastCoreCall_[3];

		// Count of clients that have performed a core call
		int coreCallPerformedCount_;

		SharedLobbyMemory* sharedLobbyMemory_ = nullptr;

		// The id of the lobby
		const int id_;
		// The name of the lobby
		const std::string nameTag_;

		// Generate a session log
		std::string sessionFile_;

		std::shared_ptr<spdlog::sinks::rotating_file_sink<std::mutex>> sessionFileSink_;

	public:
		// Lobby specific parameters

		// Points to next lobby
		Lobby* next = nullptr;
		// Points to previous lobby
		Lobby* prev = nullptr;
	};

}