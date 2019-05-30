#include "pch.h"
#include "Core.h"
#include "RconClient.h"
#include "utilities.h"

//https://www.ibm.com/support/knowledgecenter/en/ssw_ibm_i_72/rzab6/xnonblock.htm

hgs::Core::Core() {

	conf_ = SetupConfig();
	clientIndex_ = conf_.clientStartIdAt;

	fileSink_ = SetupSpdlog();

	std::string loggerName = "Core";
	log_ = utilities::SetupLogger(loggerName, fileSink_);

	log_->info("Version: 0.4");

	listener_ = SetupWinSock();
	if (listener_ == NULL) {
		ready = false;
		return;
	}

	if (conf_.rconEnable) {
		rconListener_ = SetupRcon();
		if (rconListener_ == NULL) {
			ready = false;
			return;
		}	
	}

	SetupSessionDir();

	// Create main lobby
	// TODO create main lobby

	seed_ = utilities::Random();
}

hgs::Core::~Core() {
	
}

std::shared_ptr<spdlog::sinks::rotating_file_sink<std::mutex>> hgs::Core::SetupSpdlog() const {
	const std::string logFilePath = conf_.logPath;

	// If log directory does not exists it is created
	std::experimental::filesystem::create_directory(logFilePath);

	// Global spdlog settings
	spdlog::flush_every(std::chrono::seconds(4));
	spdlog::flush_on(spdlog::level::warn);
	spdlog::set_pattern("[%a %b %d %H:%M:%S %Y] [%L] %^%n: %v%$");
	
	// Create global sharedFileSink
	try {
		
		const auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(logFilePath + "log.log", 1048576 * 5, 3);

		return fileSink;
	} catch (spdlog::spdlog_ex&) {
		std::cout << "Error, could not create logger. Is the logPath set correctly?" << std::endl;
		throw std::exception();
	}
}

void hgs::Core::CleanUp() const {
	// Clean up server
	WSACleanup();

}

hgs::Configuration hgs::Core::SetupConfig() const {
	std::cout << "Loading configuration file..." << std::endl;

	Configuration configuration;

	try { // TODO remove try catch, check if folder exists instead
		// Load content from conf file
		scl::config_file file("server.conf", scl::config_file::READ);

		for (std::pair<std::string, std::string> setting : file) {
			std::string& selector = setting.first;
			std::string& value = setting.second;

			if (selector == "server_port") {
				configuration.serverPort = std::stoi(value);
			}
			else if (selector == "clock_speed") {
				configuration.clockSpeed = std::stoi(value);
			}
			else if(selector == "socket_processing_max") {
				configuration.socketProcessingMax.tv_usec = std::stoi(value) * 1000;
			}
			else if (selector == "timeout_tries") {
				configuration.timeoutTries = std::stoi(value);
			}
			else if (selector == "timeout_delay") {
				configuration.timeoutDelay = std::stof(value);
			}
			else if (selector == "max_connections") {
				configuration.maxConnections = std::stoi(value);
			}
			else if (selector == "rcon.enable") {
				configuration.rconEnable = (value == "true");
			}
			else if (selector == "rcon.port") {
				configuration.rconPort = std::stoi(value);
			}
			else if (selector == "rcon.password") {
				configuration.rconPassword = value;
			}
			else if (selector == "log_path") {
				configuration.logPath = value;
			}
			else if (selector == "rcon.max_connections") {
				configuration.rconMaxConnections = std::stoi(value);
			}
			else if (selector == "lobby.max_connections") {
				configuration.lobbyMaxConnections = std::stoi(value);
			}
			else if (selector == "lobby.session_logging") {
				configuration.lobbySessionLogging = value == "true";
			}
			else if (selector == "lobby.start_id_at") {
				configuration.lobbyStartIdAt = std::stoi(value);
			}
			else if (selector == "lobby.session_path") {
				configuration.sessionPath = value;
			}
			else if (selector == "start_id_at") {
				configuration.clientStartIdAt = std::stoi(value);
			}
		}
		std::cout << "Configurations loaded!" << std::endl;
	
	} catch (scl::could_not_open_error&) {
		
		std::cout << "No configurations found, creating conf file..." << std::endl;

		// Create standard values

		scl::config_file file("server.conf", scl::config_file::WRITE);
		
		// Generating settings
		file.put(scl::comment(" Server settings"));
		file.put("server_port", 15000);
		file.put("clock_speed", 50);
		file.put("socket_processing_max", 1);
		file.put("timeout_tries", 30);
		file.put("timeout_delay", 0.5);
		file.put("max_connections", 10);
		file.put("rcon.enable", "false");
		file.put("rcon.port", "");
		file.put("rcon.password", "");
		file.put("rcon.max_connections", 1);
		file.put("log_path", "logs/");

		file.put(scl::comment(" Lobby settings"));
		file.put("lobby.max_connections", 5);
		file.put("lobby.start_id_at", 1);
		file.put("lobby.session_logging", "false");
		file.put("lobby.session_path", "sessions/");
		file.put(scl::comment(" Client settings"));;
		file.put("start_id_at", 1);

		// Create file
		file.write_changes();
		file.close();

		std::cout << "Configuration file created!" << std::endl;

		configuration = SetupConfig();
	}
	return configuration;
}

SOCKET hgs::Core::SetupWinSock() {

	// Initialize win sock	
	const WORD ver = MAKEWORD(2, 2);
	WSADATA wsaData;
	const int wsOk = WSAStartup(ver, &wsaData);
	if (wsOk != 0) {
		log_->error("Can't initialize winsock2");
		return NULL;
	}

	// Create listening socket
	auto listener = socket(AF_INET, SOCK_STREAM, 0);
	if (listener == INVALID_SOCKET) {
		log_->error("Can't create listening socket");
		return NULL;
	}

	// Binding connections
	sockaddr_in hint = sockaddr_in();
	hint.sin_family = AF_INET;
	hint.sin_port = htons(conf_.serverPort);
	hint.sin_addr.S_un.S_addr = INADDR_ANY;

	// Bind connections
	bind(listener, reinterpret_cast<const sockaddr*>(&hint), sizeof(hint));

	// Add listening socket
	listen(listener, SOMAXCONN);

	// Create socket array
	FD_ZERO(&sockets_);
	

	// Add listening socket to array
	FD_SET(listener, &sockets_);

	log_->info("Server seed is " + std::to_string(seed_));

	log_->info("Server port active on " + std::to_string(conf_.serverPort));

	return listener;
}

SOCKET hgs::Core::SetupRcon() const {

	// Create listening socket
	const auto listener = socket(AF_INET, SOCK_STREAM, 0);
	if (listener == INVALID_SOCKET) {
		char errorMsg[] = "Can't create rcon listening socket";
		log_->error(errorMsg);
		return NULL;
	}

	// Binding connections
	sockaddr_in hint = sockaddr_in();
	hint.sin_family = AF_INET;
	hint.sin_port = htons(conf_.rconPort);
	hint.sin_addr.S_un.S_addr = INADDR_ANY;

	// Bind connections
	bind(listener, reinterpret_cast<const sockaddr*>(&hint), sizeof(hint));

	// Add listening socket
	listen(listener, SOMAXCONN);

	// Create socket array

	FD_ZERO(&rconSockets_);

	// Add listening socket to array
	FD_SET(listener, &rconSockets_);

	log_->info("Rcon port active on: " + std::to_string(conf_.rconPort));

	if (conf_.rconPassword.length() < 4) {
		char errorMsg[] = "Rcon password must at least be 4 characters";
		log_->error(errorMsg);
		return NULL;
	}
	return listener;
}

void hgs::Core::SetupSessionDir() const {
	const std::string path = "sessions/";

	if (std::experimental::filesystem::exists(path)) {
		std::experimental::filesystem::remove_all(path);
		log_->info("Cleared previous sessions logs");
	} else {
		// If log directory does not exists it is created
		std::experimental::filesystem::create_directory(path);
		log_->info("Created session dir");
	}
}

void hgs::Core::Execute() {

	while(running_) {
		Loop();
	}
	CleanUp();
}

void hgs::Core::Loop() {

	// Duplicate master
	fd_set workingSet = sockets_;
	fd_set rconWorkingSet = rconSockets_;

	// Get socket list size
	const int result = select(0, &workingSet, nullptr, nullptr, &conf_.socketProcessingMax);
	const int rconResult = select(0, &rconWorkingSet, nullptr, nullptr, &conf_.socketProcessingMax);

	// Add new connection to main lobby
	AcceptNewConnections(result, rconResult, workingSet, rconWorkingSet);
	
	// Default sleep time between responses
	std::this_thread::sleep_for(std::chrono::milliseconds(conf_.clockSpeed));
}

void hgs::Core::AcceptNewConnections(const int select_result, const int rcon_select_result, const fd_set workingSet, fd_set rconWorkingSet) {

	// Go through all new clients to game

	for (int i = 0; i < select_result; i++) {

		const SOCKET socket = workingSet.fd_array[i];

		if (socket == listener_) {

			// Check for new connections
			const SOCKET newConnection = accept(listener_, nullptr, nullptr); // TODO second parameter has ip adress

			// Check if max clients is reached
			if (conf_.maxConnections <= connections_) {
				// Immediately close socket if max connections is reached
				closesocket(newConnection);
				break;
			}

			AddConnection(newConnection);

			// Create and connect it to main lobby
			auto* clientObject = new Client(newConnection, sharedMemory_, clientIndex_, sharedMemory_->GetMainLobby()->GetId());

			// Create a setup message
			const std::string welcomeMsg = "Successfully connected to server|" + std::to_string(clientIndex_) + "|" + std::to_string(seed_);

			// Send the message to the new client
			send(newConnection, welcomeMsg.c_str(), static_cast<int>(welcomeMsg.size()) + 1, 0);

			// Console message
			log_->info("Client#" + std::to_string(newConnection) + " connected to the server");

			// Console message
			log_->info("Client#" + std::to_string(newConnection) + " was assigned ID " + std::to_string(clientIndex_));

			// Connect client to main lobby
			edMemory_->GetMainLobby()->AddClient(clientObject, false); //TODO Connect client to master lobby

			// Connect the new client to a new thread
			std::thread clientThread(&Client::Loop, clientObject);
			clientThread.detach();

			// Increase client index
			clientIndex_++;
			break;
		}
	}
	// Go through all new clients to rcon
	for (int i = 0; i < rcon_select_result; i++) {
		const SOCKET socket = rconWorkingSet.fd_array[i];

		if (socket == rconListener_) {

			// Check for new connections
			const SOCKET newClient = accept(rconListener_, nullptr, nullptr);

			// Check if max clients is reached
			if (conf_.rconMaxConnections <= rconConnections_) {
				// Immediately close socket if max connections is reached
				closesocket(newClient);
				break;
			}

			// Add newClient to the socketList
			FD_SET(newClient, &rconSockets_);

			// Create rcon object
			auto* clientObject = new RconClient(newClient, rconIndex_, this, conf_.rconPassword, &rconSockets_, fileSink_);

			// Create thread for rcon object
			std::thread clientThread(&RconClient::Loop, clientObject);
			clientThread.detach();
			rconIndex_++;
			break;
		}
	}
}

void hgs::Core::AddConnection(const SOCKET socket) {
	// Add newClient to the socketList
	FD_SET(socket, &sockets_);
	// Increase connected clients number
	connections_++;
}

void hgs::Core::BroadcastCoreCall(int lobby, int receiver, int command) const {
	Lobby* current = sharedMemory_->GetFirstLobby();

	while (current != nullptr) {
		current->BroadcastCoreCall(lobby, receiver, command);
		current = current->next;
	}
}

void hgs::Core::ConsoleThread() {
	std::string command;
	while (true) {
		std::getline(std::cin, command);
		Interpreter(command);
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}
}

std::pair<int, std::string> hgs::Core::ServerCommand(std::string& command) {
	while (true) {
		if (callInterpreter_.try_lock()) {
			const std::pair<int, std::string> result = Interpreter(command);
			callInterpreter_.unlock();
			return result;
		}
	}
}

std::pair<int, std::string> hgs::Core::Interpreter(std::string& input) {

	std::vector<std::string> part;
	std::string command = input;
	std::string statusMessage;
	
	const std::regex regexCommand("[^ ]+");
	std::smatch matcher;

	// Check if input is a command call
	if (command[0] != '/') {
		return std::make_pair(1, "No command");
	}

	while (std::regex_search(command, matcher, regexCommand)) {
		for (auto addSegment : matcher) {
			// Append the commands to the list
			part.push_back(addSegment.str());
		}
		command = matcher.suffix().str();
	}

	if (part.size() >= 3 && part[0] == "/Client" && utilities::IsInt(part[1]) && part[2] == "drop") {
		// The first selector is lobby and the second is for client
		Lobby* clientLobby = nullptr;
		Client* currentClient = sharedMemory_->FindClient(std::stoi(part[1]), &clientLobby);

		if (currentClient != nullptr && clientLobby != nullptr) {
			clientLobby->DropClient(currentClient, false, true);
		} else {
			statusMessage = "Client or lobby not found";
			log_->warn(statusMessage);
			return std::make_pair(1, statusMessage);
		}
		
	} else if (part[0] == "/Lobby") {
		if (part.size() >= 2 && part[1] == "create") {
			if (part.size() >= 3) {
				if (sharedMemory_->FindLobby(part[2]) == nullptr) { // TODO POSSIBLE BUG if creating lobby with number
					sharedMemory_->AddLobby(part[2]);
				} else {
					statusMessage = "There is already a lobby with that name";
					log_->warn(statusMessage);
					return std::make_pair(1, statusMessage);
				}
			} else {
				sharedMemory_->AddLobby();
			}
		}
		else if (part.size() >= 2 && part[1] == "list") {

			// Compose lobby data
			Lobby* current = sharedMemory_->GetFirstLobby();
			std::string result = "\n===== Running lobbies =====\nLobbies running: " + std::to_string(sharedMemory_->GetLobbyCount()) + "\n";
			while (current != nullptr) {
				result.append("Lobby#" + std::to_string(current->GetId()) + 
					(!current->GetNameTag().empty() ? " [" + current->GetNameTag() + "]" : "") + 
					" (" + std::to_string(current->GetConnectedClients()) + " clients)" +
					"\n");
				current = current->next;
			}
			result.append("===========================");

			// Print data
			statusMessage = result;
			log_->info(result);
		}
		else if (part.size() >= 3 && part[2] == "list") {
			Lobby* lobby;
			if (utilities::IsInt(part[1])) {
				lobby = sharedMemory_->FindLobby(std::stoi(part[1]));
			}
			else {
				lobby = sharedMemory_->FindLobby(part[1]);
			}

			if (lobby == nullptr) {
				statusMessage = "Could not find lobby";
				log_->warn(statusMessage);
				return std::make_pair(1, statusMessage);
			}

			statusMessage = lobby->List();
		}
		else if (part.size() >= 3 && (part[2] == "start" || part[2] == "pause")) {

			Command action = (part[2] == "start" ? start : pause);

			const int id = sharedMemory_->GetLobbyId(part[1]);
			if (id == sharedMemory_->GetMainLobby()->GetId()) {
				statusMessage = "Main lobby cannot be targeted for this action";
				log_->warn(statusMessage);
				return std::make_pair(1, statusMessage);
			}
			else if (id == -1) {
				statusMessage = "Entered lobby was not found";
				log_->warn(statusMessage);
				return std::make_pair(1, statusMessage);
			}

			BroadcastCoreCall(id, 0, action);
			statusMessage = "Lobby "; 
			statusMessage.append(action == Command::start ? "started" : "paused");
			statusMessage.append("!");
			log_->info(statusMessage);
		}
		else if (part.size() >= 3 && part[2] == "drop") {

			Lobby* lobby;

			if (utilities::IsInt(part[1])) {
				lobby = sharedMemory_->FindLobby(std::stoi(part[1]));
			}
			else {
				lobby = sharedMemory_->FindLobby(part[1]);
			}
			
			if (lobby == nullptr) {
				statusMessage = "Could not find lobby";
				log_->warn(statusMessage);
				return std::make_pair(1, statusMessage);
			}
			// Can't drop main lobby
			if (lobby != sharedMemory_->GetMainLobby()) {
				sharedMemory_->DropLobby(lobby);
				return std::make_pair(0, "Lobby dropped");
			}
			statusMessage = "Can't drop main lobby";
			log_->warn(statusMessage);
			return std::make_pair(1, statusMessage);
		}
		// Second argument is lobby, third is client
		else if (part.size() >= 4 && part[2] == "summon" && utilities::IsInt(part[3])) {
			// Get targeted lobby
			const int targetedLobbyId = sharedMemory_->GetLobbyId(part[1]);

			const int clientId = std::stoi(part[3]);

			// Find the current lobby and client
			Lobby* currentLobby = nullptr;
			Lobby* targetLobby = sharedMemory_->FindLobby(targetedLobbyId);
			Client* client = sharedMemory_->FindClient(clientId, &currentLobby);

			if (client != nullptr) {
				if (targetLobby != nullptr) {
					return SharedMemory::MoveClient(currentLobby, targetLobby, client);
				} 
				statusMessage = "Lobby#" + std::to_string(clientId) + " not found";
				log_->warn(statusMessage);
				return std::make_pair(1, statusMessage);
			}
			
			statusMessage = "Client not found";
			log_->warn(statusMessage);
			return std::make_pair(1, statusMessage);
	
		} else {
			statusMessage = "No specifier";
			log_->warn(statusMessage);
			return std::make_pair(1, statusMessage);
		}
	}
	else if (part[0] == "/Stop") {
		running_ = false;
	}
	else {
		statusMessage = "Command not recognized";
		log_->warn(statusMessage);
		return std::make_pair(1, statusMessage);
	}

	return std::make_pair(0, statusMessage);
}