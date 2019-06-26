#pragma once
#include "shared_memory.h"
#include "lobby.h"
#include "utilities.h"

/**
    Client.h
    Purpose: Separate thread made for communication with external socket

    @author Hampus Hallkvist
    @version 0.2 07/05/2019
*/

namespace hgs {

	// Predefining classes
	class SharedMemory;
	class SharedLobbyMemory;

	class Client {
	public:
		Client(SOCKET socket, gsl::not_null<SharedMemory*> shared_memory, int id, int lobby_id);
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
			Retrieve all matches on a string
			multiple segments, splitting by characters
			"|{}[]"

			@return std::vector<std::string> Vector containing all segments
			of the earlier string
		 */
		std::vector<std::string> Split(std::string string) const;
		/**
			Retrieves the first match in the string

			@param string
			@param matcher
			@param regex
			@return std::pair<int, std::string> Vector containing all segments
			of the earlier string
		 */
		std::pair<bool, std::string> SplitFirst(std::string& string, std::smatch& matcher, const std::regex& regex) const;
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

		static bool IsApiCall(std::string& string);
		void PerformApiCall(std::string& call);

		// Getter
		std::string GetCommand() const { return clientCommand_; };
		State& GetState() { return state_; };
		SOCKET& GetSocket() { return socket_; };

		// Setters

		void SetCoreCall(std::vector<int>& core_call);
		void SetSocket(SOCKET socket);
		void SetId(int id);
		void SetInterval(std::chrono::microseconds microseconds);
		void SetMemory(gsl::not_null<SharedLobbyMemory*> lobby_memory);
		void SetPause(bool pause);
		void SetState(State state);
		void SetPrevState(State state);
		void SetOutgoing(std::vector<std::string>& outgoing);
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

		// Communication regex
		const std::regex comRegex_;

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

}