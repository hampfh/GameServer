#pragma once
#include <string>
#include <iostream>
#include <WS2tcpip.h>
#include <regex>
#pragma comment(lib, "ws2_32.lib")

namespace symbiosis {
	class Client {
	public:
		Client(std::string ip, int port);
		~Client();
		int Send(std::string& outgoing);
		std::string Receive();
		// Get last error from client
		void What() const;

		int GetId() const { return clientId_; };
		int GetSeed() const { return serverSeed_; };
	private:
		int Connect(std::string& ip, int port);
		void Disconnect() const;

		bool connectionEstablished_;
		int clientId_;
		unsigned int serverSeed_;
		SOCKET sock_;
		std::string lastError_;
	};
}

