#pragma once
#include "client.h"
#include "core.h"

/**
	RconClient.h
	Purpose: Separate thread made for communication with remote consoles

	@author Hampus Hallkvist
	@version 0.3 10/05/2019
*/

namespace hgs {

	class Core;

	class RconClient {
		SOCKET socket_;
		int id_;
		bool isOnline_;
		// Has the rcon connection been approved
		bool confirmed_;

		Core* core_;
		std::string outgoing_;
		std::string password_;

		fd_set* socketList_;

		// Shared pointer to logger
		std::shared_ptr<spdlog::logger> log_;
	public:
		RconClient(SOCKET socket, int id, gsl::not_null<Core*> core, std::string& password, fd_set* socket_list, std::shared_ptr<spdlog::sinks::rotating_file_sink<std::mutex>> file_sink);
		~RconClient();

		/**
			The loop for the client rcon client

			@return void
		 */
		void Loop();
		/**
			Method responsible for receiving command
			and then pass to the interpreter

			@return void
		 */
		void Receive();
		/**
			Echo interpreter response to client

			@return void
		 */
		void Send() const;

		/**
			Drops the socket and detaches
			from the socket list
		 */
		void Drop();
	};

}