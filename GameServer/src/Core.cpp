#include "Core.h"
#include <iostream>
#include <ctime>
#include <string>
#pragma comment(lib,"WS2_32")

//https://www.ibm.com/support/knowledgecenter/en/ssw_ibm_i_72/rzab6/xnonblock.htm

Core::Core() {
	// Initialize variables
	running_ = false;
	clientId_ = 0;

	const int port = 15000;

	// Initialize win sock	
	WORD ver = MAKEWORD(2, 2);
	WSADATA wsaData;
	int wsOK = WSAStartup(ver, &wsaData);
	if (wsOK != 0) {
		std::cout << "Can't initialize winsock" << std::endl;
		return;
	}

	// Create listening socket
	listening_ = socket(AF_INET, SOCK_STREAM, 0);
	if (listening_ == INVALID_SOCKET) {
		std::cout << "Can't create listening socket" << std::endl;
		return;
	}

	// Binding connections
	sockaddr_in hint;
	hint.sin_family = AF_INET;
	hint.sin_port = htons(port);
	hint.sin_addr.S_un.S_addr = INADDR_ANY;

	// Bind connections
	bind(listening_, (sockaddr*)&hint, sizeof(hint));

	// Add listening socket
	listen(listening_, SOMAXCONN);

	// Create socket array
	fd_set master;
	FD_ZERO(&master);

	// Add listening socket to array
	FD_SET(listening_, &master);

	// Generate server seed
	srand(time(nullptr));
	const int serverSeed = rand() % 100000;
	std::cout << "Server seed is: " << serverSeed << std::endl;
	seed_ = serverSeed;

	std::cout << "Server port is: " << port << std::endl;

	// Instantiate shared memory
	sharedMemory_ = new SharedMemory;

	// Assign
	sharedMemory_->AddSocketList(master);
}


Core::~Core() {
	// Clean up server
	delete sharedMemory_;
}

void Core::Execute() {

	running_ = true;
	std::cout << "Server Starting" << std::endl;
	timeInterval_.tv_usec = 1000;

	while(running_) {
		Loop();
	}
	CleanUp();
}

void Core::Loop() {

	std::cout << "LOOP" << std::endl;
	// Duplicate master
	fd_set copy = *sharedMemory_->GetSockets();

	// Get socket list size
	sharedMemory_->SetConnectedClients(select(0, &copy, nullptr, nullptr, &timeInterval_));

	// Run states

	// Receiving state
	if (serverState_ == 0) {
		InitializeReceiving();
	}
	// Sending state
	else if (serverState_ == 1) {
		InitializeSending();
	}

	// Swap server state
	serverState_ = (serverState_ == 0 ? 1 : 0);

	std::cout << "SERVER LOOPING" << std::endl;

	// Default sleep time between responses
	Sleep(100);
}

void Core::CleanUp() {

}

void Core::InitializeReceiving() {

	sharedMemory_->SetState(State::receiving);

	for (int i = 0; i < sharedMemory_->GetConnectedClients(); i++) {

		const SOCKET socket = sharedMemory_->GetSockets()->fd_array[i];

		if (socket == listening_) {
			std::cout << "1" << std::endl;
			// Check for new connections
			const SOCKET newClient = accept(listening_, nullptr, nullptr);
			std::cout << "2" << std::endl;
			sharedMemory_->AddSocket(newClient);
			std::cout << "3" << std::endl;
			// Add newClient to socketContentList
			clientId_++;

			// TODO Assign socket to client class
			// Create new client object
			Client* clientObject = new Client(newClient, sharedMemory_, clientId_);
			std::cout << "4" << std::endl;
			// TODO Create thread and add clientObject to it
			// Connect the new client to a new thread
			std::thread clientThread(&Client::Loop, clientObject);
			clientThread.detach();
			std::cout << "5" << std::endl;
			// Create a setup message
			std::string welcomeMsg = "Successfully connected to server";
			// Send the message to the new client
			send(newClient, welcomeMsg.c_str(), welcomeMsg.size() + 1, 0);

			// Console message
			std::cout << "SERVER> Client#" << newClient << ": connected to server\n";

			send(newClient, std::to_string(clientId_).c_str(), std::to_string(clientId_).size() + 1, 0);
			// wait one millisecond and then send another message
			Sleep(1);
			send(newClient, std::to_string(seed_).c_str(), std::to_string(seed_).size() + 1, 0);
			// Console message
			std::cout << "SERVER> Client#" << newClient << ": was assigned ID " << clientId_ << std::endl;
			break;
		}
		std::cout << "RECEIVE FINISHED" << std::endl;
	}

	int tempCount = 0;
	while (true) {
		// Check if all clients have received their payload
		std::cout << "SERVER> CLIENTS READY: " << sharedMemory_->GetReadyClients() << std::endl;
		std::cout << "SERVER> CLIENTS CONNECTED: " << sharedMemory_->GetConnectedClients() << std::endl;
		if (sharedMemory_->GetReadyClients() == sharedMemory_->GetConnectedClients()) {
			sharedMemory_->SetState(State::awaiting);
			return;
		}
		if (tempCount > 100) {
			sharedMemory_->SetState(State::awaiting);
			return;
		}
		tempCount++;
		Sleep(1);
	}
}

void Core::InitializeSending() const {
	std::cout << "SERVER> SENDING STATE" << std::endl;
	sharedMemory_->SetState(State::sending);

	int tempCount = 0;
	while (true) {
		// Check if all clients have received their payload
		if (sharedMemory_->GetReadyClients() == sharedMemory_->GetConnectedClients()) {
			sharedMemory_->SetState(State::awaiting);
			return;
		}
		if (tempCount > 100) {
			std::cout << "SERVER> SENDING TIMED OUT" << std::endl;
			sharedMemory_->SetState(State::awaiting);
			return;
		}
		tempCount++;
		Sleep(1);
	}
}

void Core::Interpreter() {
	std::string command;
	while (true) {
		// TODO add an interpreter for the server commands
		std::cin >> command;
		std::cout << command << std::endl;
	}
}
