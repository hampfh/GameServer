#pragma once
#include <mutex>
#include "pch.h"

enum State {
	none = 0,
	awaiting = 1,
	receiving = 2,
	sending = 3
};

enum Command {
	start = 0,
	kick = 1
};

class SharedMemory {
public:
	SharedMemory(std::shared_ptr<spdlog::sinks::rotating_file_sink<std::mutex>> shared_file_sink);
	~SharedMemory();
	/**
		A method called by client thread
		to tell the core that they are 
		ready for the next iteration

		@return void
	 */
	void Ready();
	/**
		Add the command and appends it to 
		the shared memory's command queue.
		A header is also applied in front
		of the command with the id of the
		sender

		@param id Id of the client sending
		@param command Command to perform
		@return void
	 */
	void AppendClientCommands(int id, std::string command);
	/**
		Method appends an internal or
		external command for the clients
		depending on the receiver

		@param receiver Id of the receiving thread, 0 for broadcast 
		@param command The command to execute, using the "Command" enum
		@return void
	 */
	void AddCoreCall(int receiver, int command);
	/**
		Tell the shared memory 
		that client has interpreted
		and performed the core call

		@return void
	 */
	void PerformedCoreCall();
	/**
		Clears the core call from
		all commands and resets 
		the "coreCallPerformedCount_"
		receiving count

		@return void
	 */
	void ResetCoreCall();
	/**
		Add a socket to the shared
		memory and increase the
		connected client count

		@return void
	 */
	void AddSocket(SOCKET new_socket);
	/**
		Drop as specific socket from
		the shared memory and decrease
		the connected client count

		@return void
	 */
	void DropSocket(SOCKET socket);

	/**
		Clears the vector array
		from all previous commands

		@return void
	 */
	void ResetCommandQueue();

	// Getters

	State GetState() const { return serverState_; };
	int GetConnectedClients() const { return connectedClients_; };
	int GetReadyClients() const { return clientsReady_; };
	fd_set* GetSockets() { return &sockets_; };
	std::vector<std::string> GetClientCommands() const { return commandQueue_; };
	std::vector<std::vector<int>> GetCoreCall() const { return coreCall_; };

	// Setters

	void SetState(State new_state);
	void SetConnectedClients(int connected_clients);
	void SetSockets(fd_set list);

private:
	// A collection of all sockets
	fd_set sockets_;
	State serverState_;

	// Counts the number of ready clients
	int clientsReady_;
	// Connected clients to server count
	int connectedClients_;

	// Shared pointer to logger
	std::shared_ptr<spdlog::logger> log_;

	// Mutex objects used to for thread safety 

	std::mutex clientStateMtx_;
	std::mutex addCommandMtx_;
	std::mutex addSocketMtx_;
	std::mutex dropSocketMtx_;
	std::mutex coreCallMtx_;

	// Dynamic allocated array holding all clients responses
	std::vector<std::string> commandQueue_;
	// Dynamic allocated array holding all core calls
	std::vector<std::vector<int>> coreCall_;
	// Count of clients that have performed a core call
	int coreCallPerformedCount_;
public:
	const std::shared_ptr<spdlog::sinks::rotating_file_sink<std::mutex>> sharedFileSink;
};

