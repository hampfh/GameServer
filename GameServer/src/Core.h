#pragma once
#include "SharedMemory.h"

/**
    Core.h
    Purpose: Main thread. This thread controls all other threads and delegates work 

    @author Hampus Hallkvist
    @version 0.5 08/04/2019
*/

class Core {
public:
	Core();
	~Core();
	/**
		Cleanup calls winsock2 cleanup, deletes
		the shared memory and lastly delete the
		core class itself

		@return void
	 */
	void CleanUp() const;
	/**
		Setup method for configuration file

		@return void
	 */
	void SetupConfig();
	/**
		Setup method for winsock2, creating all 
		necessary variables and dependencies for
		communication between sockets

		@return void
	 */
	void SetupWinSock();

	/**
		Method is the main loop of the program.
		It calls the loop method every iteration
		and does a cleanup on finish

		@return void
	 */
	void Execute();
	/**
		The loop of the program performing all
		necessary tasks every iteration of the
		program

		@return void
	 */
	void Loop();
	/**
		Initializes the receiving state, telling 
		client threads start receiving data from 
		their corresponding socket

		@param select_result winsock2 select() response
		@return void
	 */
	void InitializeReceiving(int select_result);
	/**
		Broadcasts the call from core
		to all lobbies

		@param lobby Id of the lobby
		@param receiver Id of the targeted client
		@param command Type of command
		@return void
	 */
	void BroadcastCoreCall(int lobby, int receiver, int command) const;
	/**
		The interpreter is ran by another thread
		to interpret and execute commands entered 
		in the server console 
		

		@return void
	 */
	void Interpreter();
private:

	bool running_;

	int port_;

	SOCKET listening_;

	// Id index
	int clientIndex_;
	// Maximum connections the server will allow
	int maxConnections_;

	unsigned int seed_;

	// Shared pointer to logger
	std::shared_ptr<spdlog::logger> log_;

	timeval timeInterval_;

	fd_set workingSet_;

	SharedMemory* sharedMemory_ = nullptr;
};

