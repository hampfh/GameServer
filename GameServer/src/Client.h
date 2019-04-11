#pragma once
#include "SharedMemory.h"
#include "Lobby.h"

/**
    Client.h
    Purpose: Separate thread made for communication with external socket

    @author Hampus Hallkvist
    @version 0.5 08/04/2019
*/

// Predefining class
class SharedMemory;

class Client {
public:
	Client(SOCKET socket, SharedMemory* shared_memory, int id);
	~Client();

	/**
		The loop fo the client thread, this alternates 
		between sending and receiving mode
		
		@return void 
	 */
	void Loop();
	/**
		Method responsible for receiving information 
		from external socket. After receiving data the 
		information is passed to the shared memory
		
		@return void 
	 */
	void Receive();
	/**
		Method responsible for compiling data from 
		sharedMemory and core and later send that 
		to external socket
		
		@return void 
	 */
	void Send();
	/**
		Method responsible for interpreting and performing 
		calls from the core thread
		
		@return void 
	 */
	void CoreCallListener();
	/**
		The interpreter separates a string into
		multiple segments, splitting by characters
		"|{}[]"
		
		@return std::vector<std::string> Vector containing all segments
		of the earlier string
	 */
	std::vector<std::string> Interpret(std::string string) const;
	/**
		The Drop method removes the socket from the server register, 
		drops the client and stop the external thread

		@return void
	 */
	void RequestDrop() const;

	// Getter
	std::string GetCommand() const { return clientCommand_; };
	State& GetState() { return state_; };

	// Setters

	void SetSocket(SOCKET socket);
	void SetId(int id);
	void SetInterval(std::chrono::microseconds microseconds);
	void SetLobbyStateReference(State* lobby_state);
	void SetLobbyDropReference(std::vector<int>* drop_list);
	void SetState(State state);
private:
	
	// Alive status of the socket
	bool isOnline_;

	SOCKET socket_;

	std::chrono::microseconds loopInterval_;

	// Shared pointer to logger
	std::shared_ptr<spdlog::logger> log_;

	// Received response from client socket
	std::string clientCommand_;
	// Awaiting commands for coreCall
	std::string pendingSend_;

	// Dynamic allocated array holding outgoing commands
	std::vector<std::string> outgoingCommands_;

	State state_;
	State* lobbyState_ = nullptr;

	SharedMemory* sharedMemory_ = nullptr;

	// A pointer to awaiting kick in lobby
	std::vector<int>* dropList_ = nullptr;
public:
	// Client specifiers

	// The id of the client
	int id;
	// Points to next lobby
	Client* next = nullptr;
	// Points to previous lobby
	Client* prev = nullptr;

};

