#include "serverCon.h"

symbiosis::Client::Client(std::string ip, const int port) {
	serverSeed_ = 0;
	clientId_ = 0;
	sock_ = socket(NULL, SOCK_STREAM, NULL);

	if (Connect(ip, port) != 0) {
		connectionEstablished_ = false;
		lastError_ = "Could not connect to server";
	} else {
		connectionEstablished_ = true;
	}
}


symbiosis::Client::~Client() {
	if (connectionEstablished_)
		Disconnect();
}

int symbiosis::Client::Connect(std::string& ip, const int port) {
	// Initialize win sock
	WSADATA data;
	WORD ver = MAKEWORD(2, 2);
	int wsResult = WSAStartup(ver, &data);
	if (wsResult != 0) {
		std::cerr << "Can't start winsock2, Err #" << wsResult << std::endl;
		return 1;
	}

	// Create socket
	sock_ = socket(AF_INET, SOCK_STREAM, 0);
	if (sock_ == INVALID_SOCKET) {
		std::cerr << "Can't create socket, Err #" << WSAGetLastError() << std::endl;
		WSACleanup();
		return 1;
	}

	// Fill in an int structure
	sockaddr_in hint = sockaddr_in();
	hint.sin_family = AF_INET;
	hint.sin_port = htons(port);
	inet_pton(AF_INET, ip.c_str(), &hint.sin_addr);

	// Connect to server
	int connResult = connect(sock_, reinterpret_cast<const sockaddr*>(&hint), sizeof(hint));
	if (connResult == SOCKET_ERROR) {
		std::cerr << "Can't connect to server, Err #" << WSAGetLastError() << std::endl;
		closesocket(sock_);
		WSACleanup();
		return 1;
	}

	// Client receives welcome message, id and the server seed
	std::string incoming = Receive();

	const std::regex regexClient("[^\\|]+");
	std::smatch matcher;

	std::vector<std::string> segment;

	// Interpret incoming message
	while (std::regex_search(incoming, matcher, regexClient)) {
		segment.push_back(matcher[0].str());
		incoming = matcher.suffix().str();
	}

	clientId_ = std::stoi(segment[1]);
	serverSeed_ = static_cast<unsigned int>(std::stoul(segment[2]));

	return 0;
}

void symbiosis::Client::Disconnect() const {
	if (connectionEstablished_) {
		closesocket(sock_);
		WSACleanup();
	}
}

int symbiosis::Client::Send(std::string& outgoing) {
	if (connectionEstablished_) {
		if (send(sock_, outgoing.c_str(), outgoing.size() + 1, 0) == SOCKET_ERROR) {
			lastError_ = "Send could not be performed";
			std::cout << "ERROR: " << WSAGetLastError() << std::endl;
			return 1;
		}
	}
	return 0;
}

std::string symbiosis::Client::Receive() {
	if (connectionEstablished_) {
		char incoming[1024];
		ZeroMemory(incoming, 1024);

		const int bytes = recv(sock_, incoming, 1024, 0);

		if (bytes < 0) {
			lastError_ = "No response from server";
			return "";
		}
		return std::string(incoming, static_cast<const std::basic_string<char>::size_type>(bytes) + 1);
	}
	return "";
}

void symbiosis::Client::What() const {
	if (lastError_.empty() || lastError_.length() <= 1) {
		return;
	}
	std::cout << "Last error: " << lastError_ << std::endl;
}
