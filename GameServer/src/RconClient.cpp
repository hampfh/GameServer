#include "pch.h"
#include "RconClient.h"

hgs::RconClient::RconClient(const SOCKET socket, const int id, const gsl::not_null<Core*> core, std::string& password, fd_set* socket_list, const std::shared_ptr<spdlog::sinks::rotating_file_sink<std::mutex>> file_sink) :
	socket_(socket), id_(id), core_(core), password_(password), socketList_(socket_list) {

	isOnline_ = true;
	confirmed_ = false;

	// Setup client logger
	std::vector<spdlog::sink_ptr> sinks;
	sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
	sinks.push_back(file_sink);
	
	log_ = std::make_shared<spdlog::logger>("Rcon#" + std::to_string(socket), begin(sinks), end(sinks));
	log_->set_pattern("[%a %b %d %H:%M:%S %Y] [%L] %^%n: %v%$");
	spdlog::register_logger(log_);

	log_->info("Rcon session started");
}

hgs::RconClient::~RconClient() {
	isOnline_ = false;
	spdlog::drop("Rcon#" + std::to_string(socket_));
	closesocket(socket_);
}


void hgs::RconClient::Loop() {
	while (isOnline_) {
		Receive();
		Send();
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
	delete this;
}

void hgs::RconClient::Receive() {
	char incoming[1024];

	// Clear the storage before usage
	ZeroMemory(incoming, 1024);

	// Receive 
	const int bytes = recv(socket_, incoming, 1024, 0);

	// Check if client responds
	if (bytes <= 0) {
		// Disconnect client
		log_->warn("Lost connection to rcon");
		Drop();

		return;
	}

	std::string rconCommand = std::string(incoming, strlen(incoming));

	// Check if password is correct
	if (!confirmed_) {
		if (rconCommand == password_) {
			confirmed_ = true;
			log_->info("Connection approved, assigned id " + std::to_string(id_));
			outgoing_ = "Connection approved";
			return;
		} else {
			log_->error("Not authorized");
			outgoing_ = "Permission denied";
			Send();
			Drop();
			return;
		}
	}

	// Execute rcon command
	const std::pair<int, std::string> result = core_->ServerCommand(rconCommand);
	rconCommand = result.second;

	if (result.first == 0) {
		log_->info("Performed remote command");
	}
	outgoing_ = rconCommand;
}

void hgs::RconClient::Send() const {
	
	// Send response
	// TODO encode output using compressor tool
	send(socket_, outgoing_.c_str(), static_cast<int>(outgoing_.size()) + 1, 0);
}

void hgs::RconClient::Drop() {
	closesocket(socket_);
	FD_CLR(socket_, socketList_);
	isOnline_ = false;
	log_->info("Dropped rcon");
}