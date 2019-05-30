#pragma once
#include "shared_memory.h"
#include "client.h"
#include "utilities.h"

/**
	Lobby.h
	Purpose: The lobby is a way of grouping clients to each other. 
	This enables the server to have multiple clients running in different sections without interrupting each other

	@author Hampus Hallkvist
	@version 0.2 07/05/2019
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

		// Getters

		State GetState() const { return state_; };
		State GetNextState() const { return nextState_; };
		State* GetStatePointer() { return &state_; };
		std::vector<int> GetDropList() const { return dropList_; };
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
		const int id_;

		std::mutex addDropMtx_;
		std::mutex setPauseMtx_;
	};

	class Lobby {
	public:
		Lobby(int id, std::string& name_tag, gsl::not_null<SharedMemory*> shared_memory, Configuration* conf);
		~Lobby();
		/**
			Cleanup lobby and delete drop all
			connections

			@return void
		 */
		void CleanUp();
		/**
			Method is the main loop for the lobby.
			It calls the loop method every iteration
			and does a cleanup on finish

			@return void
		 */
		void Execute();
		/**
			The lobby loop making sure
			all clients are synced and
			are working together

			@return void
		 */
		void Loop();
		/**
			Initializes the sending state, telling
			client threads to send their data to
			their corresponding socket

			@return void
		 */
		void InitializeSending();
		/**
			Initializes the receiving state, telling
			client threads start receiving data from
			their corresponding socket

			@return void
		 */
		void InitializeReceiving();
		/**
			Drops all clients with a specific state
			@param non_condition_state Drops all clients which is not this state
			@return void
		 */
		void DropNonResponding(State non_condition_state);
		/**
			Broadcasts the call from lobby
			to all clients

			@param lobby Id of the lobby
			@param receiver Id of the targeted client
			@param command Type of command
			@return void
		 */
		void BroadcastCoreCall(int& lobby, int& receiver, int& command);
		/**
			This method will wait until a certain selector becomes
			is equal to the condition
			@param perform_on_state The state where the pause commands will be performed on. Null will perform pause on any state
			@return void
		 */
		void WaitForPause(State perform_on_state) const;
		void WaitForPause() const;
		/**
			This method will list all
			clients in a specific lobby
			@return void
		 */
		std::string List() const;
		/**
			Iterate through the lobby to try to find if a specific client
			is withing it

			@param id Id of the client to find
			@return Client* if client is found, otherwise nullptr
		 */
		Client* FindClient(int id) const;

		/**
			Add a client to the lobby

			@param client Connect client object to lobby
			@param respect_limit Decides if the lobby should accept clients
			even if the lobby is full
			@param external Is the method called from outside the class
			@return int 0 if client was added successfully, 1 if lobby is full
		*/
		int AddClient(Client* client, bool respect_limit = true, bool external = false);
		/**
			Drops and removes a specific
			client from the list

			@param id If of client to drop
			@param detach_only Determines if the lobby should only disconnect
			the client from itself or actually remove it
			@param external Is the method called from outside the class
			@return Client* Will return client if detached and nullptr
			on full delete or if not found
		*/
		Client* DropClient(int id, bool detach_only = false, bool external = false);
		/**
			Drops and removes a specific
			client from the list

			@param client A pointer to the client
			@param detach_only Determines if the lobby should only disconnect
			the client from itself or actually remove it
			@param external Is the method called from outside the class
			@return Client* Will return client if detached and nullptr
			on full delete or if not found
		*/
		Client* DropClient(Client* client, bool detach_only = false, bool external = false);
		/**
			Drop all clients awaiting
			drop

			@return void
		 */
		void DropAwaiting();
		/**
			A command called to drop
			the lobby. When called it
			stops the loop and performs
			a cleanup

			@return void
		 */
		void Drop();
		/**
			This method drops all clients connected to the lobby
			@return int
		 */
		int DropAll();

		// Getters
		std::vector<std::string> GetClientCommands() const { return commandQueue_; };
		int GetConnectedClients() const { return connectedClients_; };
		int GetId() const { return id_; };
		std::string GetNameTag() const { return nameTag_; };
	private:
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

		SharedMemory* sharedMemory_ = nullptr;
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