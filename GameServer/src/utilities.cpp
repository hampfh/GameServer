#include "pch.h"
#include "utilities.h"


bool hgs::utilities::IsInt(std::string& string) {
	try {
		std::stoi(string);
		return true;
	}
	catch (...) { return false; }
}		

std::shared_ptr<spdlog::logger> hgs::utilities::SetupLogger(std::string& logger_name, const std::shared_ptr<spdlog::sinks::rotating_file_sink<std::mutex>> file_sink) {
	// Setup client logger
	std::vector<spdlog::sink_ptr> sinks;
	sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
	sinks.push_back(file_sink);

	std::shared_ptr<spdlog::logger> logger = std::make_shared<spdlog::logger>(logger_name, begin(sinks), end(sinks));
	logger->set_pattern("[%a %b %d %H:%M:%S %Y] [%L] %^%n: %v%$");
	register_logger(logger);

	logger->info(logger_name + " logger started");

	return logger;
}

int hgs::utilities::Random() {
	std::random_device rd;
	default_random_engine rand(rd());
	return static_cast<int>(rand());
}