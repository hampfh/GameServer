#include "pch.h"
#include "Core.h"

//https://www.ibm.com/support/knowledgecenter/en/ssw_ibm_i_72/rzab6/xnonblock.htm

Core::Core() {
	// ### Setup logging ###

	// Connecting loggers to same output file
	const auto sharedFileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>("logs/log.log", 1048576 * 5, 3);

	// Setup core logger
	std::vector<spdlog::sink_ptr> sinks;
	sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
	sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>("logs/log.log", 1048576 * 5, 3));
	log_ = std::make_shared<spdlog::logger>("Core", begin(sinks), end(sinks));
	register_logger(log_);

	sinks.clear();

	// Setup client logger
	sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
	sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>("logs/log.log", 1048576 * 5, 3));
	register_logger(std::make_shared<spdlog::logger>("Client", begin(sinks), end(sinks)));

	// Global spdlog settings
	spdlog::flush_on(spdlog::level::info);
	spdlog::set_pattern("[%a %b %d %H:%M:%S %Y] [%Lf] %^%n: %v%$");

	// ### Setup winsock ###

	// Initialize variables
	running_ = false;
	clientId_ = 0;

	const int port = 15000;

	// Initialize win sock	
	const WORD ver = MAKEWORD(2, 2);
	WSADATA wsaData;
	const int wsOk = WSAStartup(ver, &wsaData);
	if (wsOk != 0) {
		log_->error("Can't initialize winsock");
		return;
	}

	// Create listening socket
	listening_ = socket(AF_INET, SOCK_STREAM, 0);
	if (listening_ == INVALID_SOCKET) {
		log_->error("Can't create listening socket");
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
	workingSet_ = master;

	// Add listening socket to array
	FD_SET(listening_, &master);

	// Generate server seed
	srand(time(nullptr));
	const int serverSeed = rand() % 100000;

	log_->info("Server seed is " + std::to_string(serverSeed));

	seed_ = serverSeed;

	log_->info("Server port is " + std::to_string(port));

	// Instantiate shared memory
	sharedMemory_ = new SharedMemory;

	// Assign
	sharedMemory_->AddSocketList(master);

	// Other assignments
	timeInterval_.tv_usec = 1000;
	running_ = true;
}


Core::~Core() {
	// Clean up server
	WSACleanup();
	delete sharedMemory_;
}

void Core::Execute() {

	log_->info("Server Starting");

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
	InitializeReceiving();
	// Sending state
	InitializeSending();

	// Swap server state
	serverState_ = (serverState_ == 0 ? 1 : 0);

	// Default sleep time between responses
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

void Core::CleanUp() {
	
}


void Core::InitializeReceiving() {

	sharedMemory_->SetState(receiving);

	// Clear the coordinate storage
	sharedMemory_->Reset();

	for (int i = 0; i < sharedMemory_->GetConnectedClients(); i++) {

		const SOCKET socket = workingSet_.fd_array[i];

		if (socket == listening_) {

			// Check for new connections
			const SOCKET newClient = accept(listening_, nullptr, nullptr);

			sharedMemory_->AddSocket(newClient);

			// Add newClient to socketContentList
			clientId_++;

			// Create new client object
			auto* clientObject = new Client(newClient, sharedMemory_, clientId_);

			// Connect the new client to a new thread
			std::thread clientThread(&Client::Loop, clientObject);
			clientThread.detach();

			// Create a setup message
			std::string welcomeMsg = "Successfully connected to server";
			// Send the message to the new client
			send(newClient, welcomeMsg.c_str(), welcomeMsg.size() + 1, 0);

			// Console message
			log_->info("Client#" + std::to_string(newClient) + " connected to the server");

			send(newClient, std::to_string(clientId_).c_str(), std::to_string(clientId_).size() + 1, 0);
			// wait one millisecond and then send another message
			Sleep(1);
			send(newClient, std::to_string(seed_).c_str(), std::to_string(seed_).size() + 1, 0);
			// Console message
			log_->info("Client#" + std::to_string(newClient) + " was assigned ID " + std::to_string(clientId_));
			break;
		}
	}

	for (int i = 0; i < 100; i++) {
		// Check if all clients have received their payload
		if (sharedMemory_->GetReadyClients() == sharedMemory_->GetConnectedClients()) {
			sharedMemory_->SetState(awaiting);
			return;
		}
		// Sleep for half a millisecond
		std::this_thread::sleep_for(std::chrono::microseconds(500));
	}
	sharedMemory_->SetState(awaiting);
	log_->warn("[State Receive] Timed out");
}

void Core::InitializeSending() const {
	sharedMemory_->SetState(sending);

	for (int i = 0; i < 100; i++) {
		// Check if all clients have received their payload
		if (sharedMemory_->GetReadyClients() == sharedMemory_->GetConnectedClients()) {
			sharedMemory_->SetState(awaiting);
			return;
		}
		// Sleep for half a millisecond
		std::this_thread::sleep_for(std::chrono::microseconds(500));
	}
	log_->warn("[State Sending] Timed out");
	sharedMemory_->SetState(awaiting);
}

void Core::Interpreter() {
	std::string command;
	while (true) {
		// TODO add an interpreter for the server commands
		std::cin >> command;
		std::cout << command << std::endl;
	}
}
