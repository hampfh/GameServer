#pragma once
#include "Lobby.h"

/**
    SharedMemory.h
    Purpose: This class holds data and information accessible for all threads

    @author Hampus Hallkvist
    @version 0.5 08/04/2019
*/

// Predefine class
class Lobby;

class SharedMemory {
public:
	SharedMemory();
	~SharedMemory();
	/**
		Setup method for logging in the core class
		and global logging values

		@return void
	 */
	std::shared_ptr<spdlog::sinks::rotating_file_sink_mt> SetupLogging();
	/**
		Add a socket to the shared
		memory and increase the
		connected client count

		@param new_socket Socket to add to shared memory
		@return void
	 */
	void AddSocket(SOCKET new_socket);
	/**
		Drop as specific socket from
		the shared memory and decrease
		the connected client count

		@param socket Socket for to drop
		@return void
	 */
	void DropSocket(SOCKET socket);
	/**
		Creates a new separate section
		of the server, segregated from
		all other activities

		@return Lobby*
	*/
	Lobby* AddLobby();
	/**
		Create a main lobby which all
		user will be connected to
		on connect. If a main already
		exists then the method simply
		return nullptr

		@return Lobby*
	*/
	Lobby* CreateMainLobby();
	/**
		Drop a lobby and all content
		with it. This includes all
		connections and parameters

		@param id Identification of lobby
		@return void
	*/
	void DropLobby(int id);

	/**
		Checks if a string of
		characters can be converted
		to an int

		@param string String to test for convert
		@return bool
	*/
	bool IsInt(std::string& string) const;
	/**
		Method appends an internal or
		external command for the clients
		depending on the receiver

		@param receiver Id of the receiving thread, 0 for broadcast
		@param command The command to execute, using the "Command" enum
		@return void
	 */
	 void AddCoreCall(int receiver, int command);

	// Getters

	int GetConnectedClients() const { return connectedClients_; };
	fd_set* GetSockets() { return &sockets_; };
	std::shared_ptr<spdlog::sinks::rotating_file_sink<std::mutex>> GetFileSink() const { return sharedFileSink_; };
	Lobby* GetFirstLobby() const { return firstLobby_; };
	Lobby* GetLobby(int id) const;
	Lobby* GetMainLobby() const { return mainLobby_; };
	int GetTimeoutTries() const { return timeoutTries_; };
	float GetTimeoutDelay() const { return timeoutDelay_; };
	int GetClockSpeed() const { return clockSpeed_; };
	std::vector<std::vector<int>> GetCoreCall() const { return coreCall_; };

	// Setters

	void SetConnectedClients(int connected_clients);
	void SetSockets(fd_set list);
	//void SetFileSink(std::shared_ptr<spdlog::sinks::rotating_file_sink<std::mutex>> shared_file_sink);
	void SetTimeoutTries(int tries);
	void SetTimeoutDelay(float delay);
	void SetClockSpeed(int clock_speed);

private:
	// A collection of all sockets
	fd_set sockets_;

	// All clients connected to the server
	int connectedClients_ = 0;

	// Mutex objects used to for thread safety 

	std::mutex addSocketMtx_;
	std::mutex dropSocketMtx_;
	std::mutex addLobbyMtx_;
	std::mutex dropLobbyMtx_;

	// Index adding up for each connected client
	int lobbyIndex_ = 0;
	// Max number of lobbies
	int lobbyMax_ = 0;

	// Start of lobby linked list
	Lobby* firstLobby_ = nullptr;
	// End of lobby linked list
	Lobby* lastLobby_ = nullptr;
	// Main lobby
	Lobby* mainLobby_ = nullptr;

	// Shared pointer to logger
	std::shared_ptr<spdlog::logger> log_;

	// The number of tries before timeout
	int timeoutTries_ = 0;
	// Time to wait between each timeout iteration
	float timeoutDelay_ = 0;

	// Dynamic allocated array holding all core calls
	std::vector<std::vector<int>> coreCall_;

	int clockSpeed_;

	std::shared_ptr<spdlog::sinks::rotating_file_sink<std::mutex>> sharedFileSink_;
};