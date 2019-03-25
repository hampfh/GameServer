#include "pch.h"
#include "Core.h"

//https://www.ibm.com/support/knowledgecenter/en/ssw_ibm_i_72/rzab6/xnonblock.htm

Core::Core() {
	// Setup logging
	fileCore_ = spdlog::rotating_logger_mt("FileCore", "logs/rotating.txt", 1048576 * 5, 3);
	const auto clientLog = spdlog::rotating_logger_mt("FileClient", "logs/rotating.txt", 1048576 * 5, 3);
	
	conCore_ = spdlog::stdout_color_mt("ConCore");
	const auto clientConsoleLog = spdlog::stdout_color_mt("ConClient");

	spdlog::flush_every(std::chrono::seconds(5));
	spdlog::set_pattern("[%x-%H:%M:%S] %^%n: %v%$");
	spdlog::set_default_logger(conCore_);

	// Initialize variables
	running_ = false;
	clientId_ = 0;

	const int port = 15000;

	// Initialize win sock	
	WORD ver = MAKEWORD(2, 2);
	WSADATA wsaData;
	int wsOK = WSAStartup(ver, &wsaData);
	if (wsOK != 0) {
		spdlog::get("ConCore")->error("Can't initialize winsock");
		spdlog::get("Core")->error("Can't initialize winsock");
		return;
	}

	// Create listening socket
	listening_ = socket(AF_INET, SOCK_STREAM, 0);
	if (listening_ == INVALID_SOCKET) {
		spdlog::get("ConCore")->error("Can't create listening socket");
		spdlog::get("Core")->error("Can't create listening socket");
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

	spdlog::get("ConCore")->info("Server seed is " + std::to_string(serverSeed));

	seed_ = serverSeed;

	spdlog::get("ConCore")->info("Server port is " + std::to_string(port));

	// Instantiate shared memory
	sharedMemory_ = new SharedMemory;

	// Assign
	sharedMemory_->AddSocketList(master);
}


Core::~Core() {
	// Clean up server
	WSACleanup();
	delete sharedMemory_;
}

void Core::Execute() {

	running_ = true;
	spdlog::get("ConCore")->info("Server Starting");
	timeInterval_.tv_usec = 1000;

	while(running_) {
		Loop();
	}
	CleanUp();
}

void Core::Loop() {

	// Duplicate master
	workingSet_ = *sharedMemory_->GetSockets();

	// Get socket list size
	const int result = select(0, &workingSet_, nullptr, nullptr, &timeInterval_);
	sharedMemory_->SetConnectedClients(result);

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

	// Default sleep time between responses
	Sleep(1000);
}

void Core::CleanUp() {

}

void Core::InitializeReceiving() {

	sharedMemory_->SetState(State::receiving);

	for (int i = 0; i < sharedMemory_->GetConnectedClients(); i++) {

		const SOCKET socket = workingSet_.fd_array[i];

		if (socket == listening_) {

			// Check for new connections
			std::cout << listening_ << std::endl;
			const SOCKET newClient = accept(listening_, nullptr, nullptr);

			sharedMemory_->AddSocket(newClient);

			// Add newClient to socketContentList
			clientId_++;

			// Create new client object
			Client* clientObject = new Client(newClient, sharedMemory_, clientId_);

			// Connect the new client to a new thread
			std::thread clientThread(&Client::Loop, clientObject);
			clientThread.detach();

			// Create a setup message
			std::string welcomeMsg = "Successfully connected to server";
			// Send the message to the new client
			send(newClient, welcomeMsg.c_str(), welcomeMsg.size() + 1, 0);

			// Console message
			spdlog::get("ConCore")->info("Client#" + std::to_string(newClient)  + " connected to the server");

			send(newClient, std::to_string(clientId_).c_str(), std::to_string(clientId_).size() + 1, 0);
			// wait one millisecond and then send another message
			Sleep(1);
			send(newClient, std::to_string(seed_).c_str(), std::to_string(seed_).size() + 1, 0);
			// Console message
			std::cout << "SERVER> Client#" << newClient << ": was assigned ID " << clientId_ << std::endl;
			break;
		}
	}

	int tempCount = 0;
	while (true) {
		// Check if all clients have received their payload
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
