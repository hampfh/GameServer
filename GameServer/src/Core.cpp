#include "pch.h"
#include "core.h"
#include "rcon_client.h"
#include "utilities.h"

//https://www.ibm.com/support/knowledgecenter/en/ssw_ibm_i_72/rzab6/xnonblock.htm

hgs::Core::Core() {
	rconIndex_ = 1;
	rconConnections_ = 0;
	seed_ = 0;
	running_ = true;
	ready = true;

	conf_ = SetupConfig();
	clientIndex_ = conf_.clientStartIdAt;

	// Initialization
	sharedMemory_ = new SharedMemory(conf_);

	// Setup core logger
	std::vector<spdlog::sink_ptr> sinks;
	sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
	sinks.push_back(sharedMemory_->GetFileSink());
	log_ = std::make_shared<spdlog::logger>("Core", begin(sinks), end(sinks));
	log_->set_pattern("[%a %b %d %H:%M:%S %Y] [%L] %^%n: %v%$");
	register_logger(log_);

	log_->info("Version: 0.3");

	if (SetupWinSock() != 0) {
		ready = false;
		return;
	}

	if (conf_.rconEnable) {
		// Open rcon port for listening
		if (SetupRcon() != 0) {
			ready = false;
			return;
		}
	}

	SetupSessionDir();

	// Create main lobby
	if (sharedMemory_->CreateMainLobby() == nullptr) {
		ready = false;
	}
}

hgs::Core::~Core() {
	
}

void hgs::Core::CleanUp() const {
	// Clean up server
	WSACleanup();

	delete sharedMemory_;

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

int hgs::Core::SetupWinSock() {

	// Initialize win sock	
	const WORD ver = MAKEWORD(2, 2);
	WSADATA wsaData;
	const int wsOk = WSAStartup(ver, &wsaData);
	if (wsOk != 0) {
		log_->error("Can't initialize winsock2");
		return 1;
	}

	// Create listening socket
	listening_ = socket(AF_INET, SOCK_STREAM, 0);
	if (listening_ == INVALID_SOCKET) {
		log_->error("Can't create listening socket");
		return 1;
	}

	// Binding connections
	sockaddr_in hint = sockaddr_in();
	hint.sin_family = AF_INET;
	hint.sin_port = htons(conf_.serverPort);
	hint.sin_addr.S_un.S_addr = INADDR_ANY;

	// Bind connections
	bind(listening_, reinterpret_cast<const sockaddr*>(&hint), sizeof(hint));

	// Add listening socket
	listen(listening_, SOMAXCONN);

	// Create socket array
	fd_set master;
	FD_ZERO(&master);
	workingSet_ = master;

	// Add listening socket to array
	FD_SET(listening_, &master);

	// Generate server seed
	std::random_device rd;
	std::default_random_engine gen(rd());
	seed_ = gen();

	log_->info("Server seed is " + std::to_string(seed_));

	log_->info("Server port active on " + std::to_string(conf_.serverPort));

	// Assign
	sharedMemory_->SetSockets(master);
	return 0;
}

int hgs::Core::SetupRcon() {

	// Create listening socket
	rconListening_ = socket(AF_INET, SOCK_STREAM, 0);
	if (rconListening_ == INVALID_SOCKET) {
		log_->error("Can't create rcon listening socket");
		return 1;
	}

	// Binding connections
	sockaddr_in hint = sockaddr_in();
	hint.sin_family = AF_INET;
	hint.sin_port = htons(conf_.rconPort);
	hint.sin_addr.S_un.S_addr = INADDR_ANY;

	// Bind connections
	bind(rconListening_, reinterpret_cast<const sockaddr*>(&hint), sizeof(hint));

	// Add listening socket
	listen(rconListening_, SOMAXCONN);

	// Create socket array
	fd_set master;
	FD_ZERO(&master);
	rconWorkingSet_ = master;

	// Add listening socket to array
	FD_SET(rconListening_, &master);
	rconMaster_ = master;

	log_->info("Rcon port active on: " + std::to_string(conf_.rconPort));

	if (conf_.rconPassword.length() < 4) {
		log_->error("Rcon password must at least be 4 characters");
		return 1;
	}
	return 0;
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
	workingSet_ = *sharedMemory_->GetSockets();
	rconWorkingSet_ = rconMaster_;

	// Get socket list size
	const int result = select(0, &workingSet_, nullptr, nullptr, &conf_.socketProcessingMax);
	const int rconResult = select(0, &rconWorkingSet_, nullptr, nullptr, &conf_.socketProcessingMax);

	// Add new connection to main lobby
	InitializeReceiving(result, rconResult);
	
	// Default sleep time between responses
	std::this_thread::sleep_for(std::chrono::milliseconds(conf_.clockSpeed));
}

void hgs::Core::InitializeReceiving(const int select_result, const int rcon_select_result) {

	// Go through all new clients to game

	for (int i = 0; i < select_result; i++) {

		const SOCKET socket = workingSet_.fd_array[i];

		if (socket == listening_) {

			// Check for new connections
			const SOCKET newClient = accept(listening_, nullptr, nullptr);

			// Check if max clients is reached
			if (conf_.maxConnections <= sharedMemory_->GetConnectedClients()) {
				// Immediately close socket if max connections is reached
				closesocket(newClient);
				break;
			}

			sharedMemory_->AddSocket(newClient);

			// Create and connect it to main lobby
			auto* clientObject = new Client(newClient, sharedMemory_, clientIndex_, sharedMemory_->GetMainLobby()->GetId());

			// Create a setup message
			const std::string welcomeMsg = "Successfully connected to server|" + std::to_string(clientIndex_) + "|" + std::to_string(seed_);

			// Send the message to the new client
			send(newClient, welcomeMsg.c_str(), static_cast<int>(welcomeMsg.size()) + 1, 0);

			// Console message
			log_->info("Client#" + std::to_string(newClient) + " connected to the server");

			// Console message
			log_->info("Client#" + std::to_string(newClient) + " was assigned ID " + std::to_string(clientIndex_));

			// Connect client to main lobby
			sharedMemory_->GetMainLobby()->AddClient(clientObject, false);

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
		const SOCKET socket = rconWorkingSet_.fd_array[i];

		if (socket == rconListening_) {

			// Check for new connections
			const SOCKET newClient = accept(rconListening_, nullptr, nullptr);

			// Check if max clients is reached
			if (conf_.rconMaxConnections <= rconConnections_) {
				// Immediately close socket if max connections is reached
				closesocket(newClient);
				break;
			}

			// Add newClient to the socketList
			FD_SET(newClient, &rconMaster_);

			// Create rcon object
			auto* clientObject = new RconClient(newClient, rconIndex_, this, conf_.rconPassword, &rconMaster_, sharedMemory_->GetFileSink());

			// Create thread for rcon object
			std::thread clientThread(&RconClient::Loop, clientObject);
			clientThread.detach();
			rconIndex_++;
			break;
		}
	}
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
		
		std::pair<Client*, Lobby*> result = sharedMemory_->FindClient(std::stoi(part[1]));
		Client* currentClient = result.first;
		Lobby* clientLobby = result.second;

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
				if (sharedMemory_->FindLobby(part[2]) == nullptr) {
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
			Lobby* targetLobby = sharedMemory_->FindLobby(targetedLobbyId);
			std::pair<Client*, Lobby*> result = sharedMemory_->FindClient(clientId);
			Client* client = result.first;
			Lobby* currentLobby = result.second;

			if (client != nullptr) {
				if (targetLobby != nullptr) {
					return sharedMemory_->MoveClient(currentLobby, targetLobby, client);
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
	else if (part[0] == "/help") {
		std::string commands =
			"\n##### Console Commands #####\n\n\
/Client <client id> drop - Drop a specific client\n\
/Lobby\n\
   create <name> - Creates a lobby with the entered name\n\
   list - Lists all lobbies and how many clients they're holding\n\
   <lobby> list - Lists all specifications about a specific lobby, including all clients connected to it\n\
   <lobby> start - Sends a start signal to all clients in the targeted lobby\n\
   <lobby> pause - Sends a pause signal to all clients in the targeted lobby\n\
   <lobby> drop - Drops a lobby and all it's content\n\
   <lobby> summon <client> - Transfers a client from one lobby to another\n\
/Stop - Stops the server and closes all connections\n\n\
For more information about the server console visit the documentation at: https://github.com/Hampfh/GameServer/wiki/Server-Console";
		statusMessage = commands;
	}
	else {
		statusMessage = "Command not recognized";
		log_->warn(statusMessage);
		return std::make_pair(1, statusMessage);
	}

	return std::make_pair(0, statusMessage);
}