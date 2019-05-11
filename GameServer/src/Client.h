#pragma once
#include "SharedMemory.h"
#include "Lobby.h"

/**
    Client.h
    Purpose: Separate thread made for communication with external socket

    @author Hampus Hallkvist
    @version 0.2 07/05/2019
*/

// Predefining classes
class SharedMemory;
class SharedLobbyMemory;

class Client {
public:
	Client(SOCKET socket, SharedMemory* shared_memory, int id, int lobby_id);
	~Client();

	/**
		The loop for the client thread, this alternates 
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
	/**
		Method does not delete anything but instead 
		tell the thread to exit and finish everything
		by itself

		@return void
	 */
	void End();
	/**
		Removes the connection the client has
		to the lobby memory

		@return void
	 */
	void DropLobbyConnections();

	// Getter
	std::string GetCommand() const { return clientCommand_; };
	State& GetState() { return state_; };
	SOCKET& GetSocket() { return socket_; };

	// Setters

	void SetCoreCall(std::vector<int> coreCall);
	void SetSocket(SOCKET socket);
	void SetId(int id);
	void SetInterval(std::chrono::microseconds microseconds);
	void SetMemory(SharedLobbyMemory* lobby_memory);
	void SetPause(bool pause);
	//void SetLobbyStateReference(State* lobby_state);
	//void SetLobbyDropReference(std::vector<int>* drop_list);
	void SetState(State state);
	void SetPrevState(State state);
	void SetOutgoing(std::vector<std::string> outgoing);
private:
	
	// Alive status of the socket
	bool isOnline_;
	bool paused_;
	// The client thread will never delete itself if it is attached to something. When the lobby drop a client it changes the attached state via the End() method.
	bool attached_;

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
	State lastState_;

	SharedMemory* sharedMemory_ = nullptr;
	SharedLobbyMemory* lobbyMemory_ = nullptr;

	std::vector<std::vector<int>> coreCall_;
public:
	// Client specifiers

	int lobbyId;

	// The id_ of the client
	int id;
	// Points to next lobby
	Client* next = nullptr;
	// Points to previous lobby
	Client* prev = nullptr;

};

