#pragma once
#include "lobby.h"
#include "utilities.h"

/**
    Client.h
    Purpose: Separate thread made for communication with external socket

    @author Hampus Hallkvist
    @version 0.3 07/05/2019
*/

namespace hgs {

	// Predefining classes
	class SharedLobbyMemory;

	class Client {
		
		bool alive_ = true;
		bool paused_ = false;

		// Determines if the clients is connected to a lobby
		bool attached_ = false;

		SOCKET socket_;

		std::chrono::microseconds loopInterval_;

		std::shared_ptr<spdlog::logger> log_;

		// Received response from client socket
		std::string clientCommand_;
		// Awaiting commands for coreCall
		std::string pendingSend_;

		std::vector<std::string> outgoingCommands_;

		State state_ = none;
		State lastState_ = none;

		// Communication regex
		const std::regex comRegex_;

		SharedLobbyMemory* lobbyMemory_ = nullptr;

		std::vector<std::vector<int>> coreCall_;

	public:
		// Client specifiers

		int lobbyId;
		int id;
		Client* next = nullptr;
		Client* prev = nullptr;

	private:
		Client(SOCKET socket, gsl::not_null<std::shared_ptr<spdlog::sinks::rotating_file_sink<std::mutex>>>& file_sink, int id, int lobby_id);
		~Client();

		void Loop();
		void Receive();
		void CoreCallListener();
		/**
			Retrieve all matches on a string
			multiple segments, splitting by characters
			"|{}[]"

			@return std::vector<std::string> Vector containing all segments
			of the earlier string
		 */
		std::vector<std::string> Split(std::string string) const;
		std::pair<bool, std::string> SplitFirst(std::string& string, std::smatch& matcher, const std::regex& regex) const;
	public:
		void DropLobbyAssociations();
		void Send();
		void End();

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

	};

}