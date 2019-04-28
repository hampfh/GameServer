#pragma once
#include "SharedMemory.h"
#include "Client.h"

/**
	Lobby.h
	Purpose: The lobby is a way of grouping clients to each other. 
	This enables the server to have multiple clients running in different sections without interrupting each other

	@author Hampus Hallkvist
	@version 0.5 10/04/2019
*/

// Predefining classes
class SharedMemory;
class Client;

class SharedLobbyMemory {
public:
	SharedLobbyMemory();
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
	std::vector<int> GetDropList() const { return dropList_; };

	// Setters

	void SetState(State state);
private:
	State state_;
	std::mutex addDropMtx_;
	std::vector<int> dropList_;
};

class Lobby {
public:
	Lobby(int id, std::string& name_tag, int max_connections, SharedMemory* shared_memory);
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
	void InitializeSending() const;
	/**
		Initializes the receiving state, telling
		client threads start receiving data from
		their corresponding socket

		@return void
	 */
	void InitializeReceiving();
	/**
		This method is called when the lobby
		has timed out. The method will resend
		it's latest command to all clients
		which did not answer

		@return void
	 */
	void ResendReceive(int interrupted_connections);
	/**
		Clears the vector array
		from all previous commands

		@return void
	 */
	void ResetCommandQueue();
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
		@return int 0 if client was added successfully, 1 if lobby is full
	*/
	int AddClient(Client* client, bool respect_limit = true);
	/**
		Drops and removes a specific
		client from the list
		
		@param id If of client to drop
		@param detach_only Determines if the lobby should only disconnect
		the client from itself or actually remove it
		@return Client* Will return client if detached and nullptr
		on full delete or if not found
	*/
	Client* DropClient(int id, bool detach_only = false);
	/**
		Drops and removes a specific
		client from the list

		@param client A pointer to the client
		@param detach_only Determines if the lobby should only disconnect
		the client from itself or actually remove it
		@return Client* Will return client if detached and nullptr
		on full delete or if not found
	*/
	Client* DropClient(Client* client, bool detach_only = false);
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
		at cleanup

		@return void
	 */
	void Drop();

	// Getters
	//State GetState() const { return lobbyState_; };
	std::vector<std::string> GetClientCommands() const { return commandQueue_; };
	int GetConnectedClients() const { return connectedClients_; };
	int GetId() const { return id_; };
	std::string GetNameTag() const { return nameTag_; };

private:
	bool running_;

	State internalState_ = none;

	// Maximum lobby connections the server will allow
	int maxConnections_;

	// All clients connected to the lobby
	int connectedClients_;

	// Points to next lobby
	Client* firstClient_ = nullptr;
	Client* lastClient_ = nullptr;

	// Shared pointer to logger
	std::shared_ptr<spdlog::logger> log_;

	// Mutex objects used to for thread safety 

	std::mutex clientStateMtx_;
	std::mutex addCommandMtx_;
	std::mutex coreCallMtx_;

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

public:
	// Lobby specific parameters
	
	// Points to next lobby
	Lobby* next = nullptr;
	// Points to previous lobby
	Lobby* prev = nullptr;
};

