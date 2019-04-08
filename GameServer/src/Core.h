#pragma once
#include "SharedMemory.h"

/**
    Core.h
    Purpose: Main thread. This thread controls all other threads and delegates work 

    @author Hampus Hallkvist
    @version 0.4 08/04/2019
*/

class Core {
public:
	Core();
	~Core();
	/**
		Setup method for configuration file

		@return void
	 */
	void SetupConfig();
	/**
		Setup method for logging in the core class
		and global logging values

		@return void
	 */
	std::shared_ptr<spdlog::sinks::rotating_file_sink_mt> SetupLogging();
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
		Cleanup calls winsock2 cleanup, deletes
		the shared memory and lastly delete the 
		core class itself

		@return void
	 */
	void CleanUp() const;
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

		@param select_result winsock2 select() response
		@return void
	 */
	void InitializeReceiving(int select_result);
	/**
		The interpreter is ran by another thread
		to interpret and execute commands entered 
		in the server console 
		

		@return void
	 */
	void Interpreter() const;
private:
	bool running_;

	SOCKET listening_;

	// Id index
	int clientId_;
	// Maximum connections the server will allow
	int maxConnections_;

	int seed_;
	State serverState_ = receiving;

	// Shared pointer to logger
	std::shared_ptr<spdlog::logger> log_;

	// The number of tries before timeout
	int timeoutTries_;
	// Time to wait between each timeout iteration
	float timeoutDelay_;

	timeval timeInterval_;

	fd_set workingSet_;

	// Class containing all information for the server. This class is passed to all threads 
	SharedMemory* sharedMemory_;

	std::chrono::milliseconds clockSpeed_;
};

