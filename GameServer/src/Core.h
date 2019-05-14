#pragma once
#include "SharedMemory.h"
#include "RconClient.h"

/**
    Core.h
    Purpose: Main thread. This thread controls all other threads and delegates work 

    @author Hampus Hallkvist
    @version 0.2 07/05/2019
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
	int SetupConfig();
	/**
		Setup method for winsock2, creating all 
		necessary variables and dependencies for
		communication between sockets

		@return void
	 */
	int SetupWinSock();
	/**
		Setup an environment for rcon, opening
		necessary ports etc

		@return void
	 */
	int SetupRcon();
	/**
		This method will delete all content of
		the session dir when starting. If this
		directory does not exists then it will be created

		@return void
	 */
	void SetupSessionDir() const;
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
	void InitializeReceiving(int select_result, int rcon_select_result);
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
		The consoleInput is ran by another thread
		to call the interpreter method
	 */
	void ConsoleThread();
	int ServerCommand(std::string* command);

	bool ready;
private:
	/**
		The interpreter is ran by another thread
		to interpret and execute commands entered 
		in the server console 
		

		@return void
	 */
	int Interpreter(std::string* callback_string);

	bool running_;


	// Rcon variables

	bool rcon_;
	int rconPort_;
	std::string rconPassword_;
	int rconMaxConnections_;
	int rconConnections_;
	SOCKET rconListening_;
	// rcon index
	int rconIndex_;
	fd_set rconMaster_;
	fd_set rconWorkingSet_;

	int port_;

	SOCKET listening_;

	std::mutex callInterpreter_;

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

