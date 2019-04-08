#include "pch.h"
#include "Core.h"

//https://www.ibm.com/support/knowledgecenter/en/ssw_ibm_i_72/rzab6/xnonblock.htm

Core::Core() {

	const auto sharedFileSink = SetupLogging();

	// Instantiate shared memory
	sharedMemory_ = new SharedMemory(sharedFileSink);

	SetupConfig();

	SetupWinSock();

	running_ = true;

}

void Core::SetupConfig() {
	log_->info("Loading configuration file...");
	try {
		// Load content from conf file
		scl::config_file file("server.conf", scl::config_file::READ);

		for (auto setting : file) {
			std::string& selector = setting.first;
			std::string& value = setting.second;

			if (selector == "clock_speed") {
				clockSpeed_ = std::chrono::milliseconds(std::stoi(value));
			}
			else if(selector == "socket_processing_max") {
				timeInterval_.tv_usec = std::stoi(value) * 1000;
			}
			else if (selector == "timeout_tries") {
				timeoutTries_ = std::stoi(value);
			}
			else if (selector == "timeout_delay") {
				timeoutDelay_ = std::stof(value);
			}
			else if (selector == "start_id_at") {
				clientId_ = std::stoi(value);
			}
			else if (selector == "max_connections") {
				maxConnections_ = std::stoi(value);
			}
		}
		log_->info("Configurations loaded!");
	} catch (scl::could_not_open_error&) {
		log_->warn("No configuration file found, creating conf file...");

		// Create standard values

		scl::config_file file("server.conf", scl::config_file::WRITE);
		
		// Generating settings
		file.put(scl::comment(" --Server settings--"));
		file.put(scl::comment(" (All settings associated with time are defined in milliseconds)"));
		file.put("clock_speed", 50);
		file.put("socket_processing_max", 1);
		file.put("timeout_tries", 30);
		file.put("timeout_delay", 0.5);
		file.put(scl::comment(" Client settings"));;
		file.put("start_id_at", 1);
		file.put("max_connections", 10);

		// Create file
		file.write_changes();
		//and close it to save memory.
		file.close();

		log_->info("Configuration file created!");

		// Assigning standard values to server
		clockSpeed_ = std::chrono::milliseconds(50);
		timeInterval_.tv_usec = 1000;
		timeoutTries_ = 30;
		timeoutDelay_ = 0.5f;
		// Client related
		clientId_ = 1;
		maxConnections_ = 10;
	}
}

std::shared_ptr<spdlog::sinks::rotating_file_sink_mt> Core::SetupLogging() {
	// Shared file sink
	const auto sharedFileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>("logs/log.log", 1048576 * 5, 3);

	// Setup core logger
	std::vector<spdlog::sink_ptr> sinks;
	sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
	sinks.push_back(sharedFileSink);
	log_ = std::make_shared<spdlog::logger>("Core", begin(sinks), end(sinks));
	register_logger(log_);

	// Global spdlog settings
	spdlog::flush_on(spdlog::level::info);
	spdlog::set_pattern("[%a %b %d %H:%M:%S %Y] [%Lf] %^%n: %v%$");

	return sharedFileSink;
}

void Core::SetupWinSock() {

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
	srand(static_cast<unsigned int>(time(nullptr)));
	const int serverSeed = rand() % 100000;

	log_->info("Server seed is " + std::to_string(serverSeed));

	seed_ = serverSeed;

	log_->info("Server port is " + std::to_string(port));

	// Assign
	sharedMemory_->AddSocketList(master);
}

Core::~Core() {
	
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

	// Receiving state
	if (serverState_ == receiving) {
		InitializeReceiving(result);
	}
	// Sending state
	else if (serverState_ == sending) {
		InitializeSending();
	}

	// Swap state
	switch (serverState_) {
	case receiving:
		serverState_ = sending;
		break;
	case sending:
		serverState_ = receiving;
		break;
	default:
		serverState_ = receiving;
		break;
	}

	// Default sleep time between responses
	std::this_thread::sleep_for(std::chrono::milliseconds(clockSpeed_));
}

void Core::CleanUp() const {
	// Clean up server
	WSACleanup();
	delete sharedMemory_;
	delete this;
}


void Core::InitializeReceiving(const int select_result) {

	sharedMemory_->SetState(receiving);

	// Clear the coordinate storage
	sharedMemory_->Reset();

	for (int i = 0; i < select_result; i++) {

		const SOCKET socket = workingSet_.fd_array[i];

		if (socket == listening_) {

			// Check for new connections
			const SOCKET newClient = accept(listening_, nullptr, nullptr);

			// Check if max clients is reached
			if (maxConnections_ <= sharedMemory_->GetConnectedClients()) {
				// Immediately close socket if max connections is reached
				closesocket(newClient);
				break;
			}

			sharedMemory_->AddSocket(newClient);

			// Add newClient to socketContentList
			clientId_++;

			// Create new client object
			auto* clientObject = new Client(newClient, sharedMemory_, clientId_);

			// Create a setup message
			std::string welcomeMsg = "Successfully connected to server";
			// Send the message to the new client
			send(newClient, welcomeMsg.c_str(), static_cast<int>(welcomeMsg.size()) + 1, 0);

			// Console message
			log_->info("Client#" + std::to_string(newClient) + " connected to the server");

			send(newClient, std::to_string(clientId_).c_str(), static_cast<int>(std::to_string(clientId_).size()) + 1, 0);
			// wait a little bit before sending the next message
			std::this_thread::sleep_for(std::chrono::microseconds(1));

			send(newClient, std::to_string(seed_).c_str(), static_cast<int>(std::to_string(seed_).size()) + 1, 0);
			// Console message
			log_->info("Client#" + std::to_string(newClient) + " was assigned ID " + std::to_string(clientId_));

			// Connect the new client to a new thread
			std::thread clientThread(&Client::Loop, clientObject);
			clientThread.detach();
			break;
		}
	}

	for (int i = 0; i < timeoutTries_; i++) {
		// Check if all clients have received their payload
		if (sharedMemory_->GetReadyClients() == sharedMemory_->GetConnectedClients()) {
			sharedMemory_->SetState(awaiting);
			return;
		}

		// Sleep for half a millisecond, convert milliseconds to microseconds
		std::this_thread::sleep_for(std::chrono::microseconds(static_cast<int>(timeoutDelay_ * 1000)));
	}
	sharedMemory_->SetState(awaiting);
	log_->warn("Timed out while waiting for client thread");
}

void Core::InitializeSending() const {
	sharedMemory_->SetState(sending);

	for (int i = 0; i < timeoutTries_; i++) {
		// Check if all clients have received their payload
		if (sharedMemory_->GetReadyClients() == sharedMemory_->GetConnectedClients()) {
			sharedMemory_->SetState(awaiting);
			return;
		}
		// Sleep for half a millisecond, convert milliseconds to microseconds
		std::this_thread::sleep_for(std::chrono::microseconds(static_cast<int>(timeoutDelay_ * 1000)));
	}

	sharedMemory_->SetState(awaiting);
	log_->warn("Timed out while waiting for client thread");
}

void Core::Interpreter() const {
	std::string command;
	while (true) {

		std::vector<std::string> part;

		std::getline(std::cin, command);
		
		const std::regex regexCommand("[^ ]+");
		std::smatch matcher;

		while (std::regex_search(command, matcher, regexCommand)) {
			if (matcher[0].str()[0] != '/') {
				// Not a command, break
				break;
			}
			for (auto addSegment : matcher) {
				// Append the commands to the list
				part.push_back(addSegment.str());
			}
			command = matcher.suffix().str();
		}

		if (!part.empty()) {
			if (part[0] == "/Start") {
				log_->info("Performing command");
				sharedMemory_->AddCoreCall(0, Command::start);
			}
			else if (part[0] == "/Kick") {
				sharedMemory_->AddCoreCall(std::stoi(part[1]), Command::kick);
			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}
}
