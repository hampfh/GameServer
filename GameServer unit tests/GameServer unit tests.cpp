#include "pch.h"
#include "CppUnitTest.h"
#include "core.h"
#include "utilities.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace game_server_tests {
	//hgs::Core* core = nullptr;
	
	TEST_CLASS(Core) {
		
	public:

		TEST_METHOD(TestCoreConstructor) {
			hgs::Core constructorCore(false);
			Assert::AreEqual(true, (constructorCore.Init() == 0));
		}

		TEST_METHOD(SetupConfig) {
			const hgs::Core confCore;
			const auto conf = confCore.SetupConfig();
			const bool result = (conf.serverPort != NULL &&
				conf.clockSpeed != NULL &&
				conf.timeoutTries != NULL &&
				conf.timeoutDelay != NULL &&
				conf.maxConnections != NULL &&
				conf.lobbyStartIdAt != NULL);

			Assert::AreEqual(true, result);
		}

		TEST_METHOD(SetupWinSock) {
			hgs::Core winSockCore;

			Assert::AreEqual(0, winSockCore.SetupWinSock());
		}
		TEST_METHOD(RconSetup) {
			hgs::Core rconCore;

			auto conf = rconCore.GetConf();
			conf.rconPassword = "PasswordForRcon"; // SetupRcon can only be ran if password is set
			rconCore.SetConf(conf);

			rconCore.SetupWinSock();
			Assert::AreEqual(0, rconCore.SetupRcon());
		}
	};

	TEST_CLASS(Utilities) {
	public:
		TEST_METHOD(IsInt) {
		
			std::string sampleTrue = "6623526";
			std::string sampleFalse = "Hello there I am a person 90";

			Assert::IsTrue(hgs::utilities::IsInt(sampleTrue) && !hgs::utilities::IsInt(sampleFalse));
		}
	};

	TEST_CLASS(SharedMemory) {
		hgs::Configuration conf_;
		hgs::SharedMemory* shared_;
	public:
		TEST_CLASS_INITIALIZE(ShareMemoryInitialize) {
			
		}

		SharedMemory::SharedMemory() {
			conf_.logPath = "logs/";
			shared_ = new hgs::SharedMemory(conf_);
		}
		SharedMemory::~SharedMemory() {
			delete shared_;
		}

		TEST_METHOD(CreateLobby) {
			const auto result = shared_->AddLobby();
			Assert::IsTrue(result != nullptr);
		}
		TEST_METHOD(FindLobby) {
		
			const auto result = shared_->AddLobby();
			const int lobbyId = result->GetId();
			Assert::IsTrue(shared_->FindLobby(lobbyId) != nullptr);
		}
		// Delete lobby with id
		TEST_METHOD(DeleteLobby) {

			const auto result = shared_->AddLobby();

			const int id = result->GetId();

			shared_->DropLobby(id);

			Assert::IsNull(shared_->FindLobby(id));
		}
	};

	TEST_CLASS(Interpreter) {
		hgs::Core core_;
		std::string lobbyTestName_ = "testLobby";
	public:
		TEST_METHOD(LobbyLifecycle) {
			// Create a lobby
			std::string command = "/lobby create " + lobbyTestName_;

			// Check for success
			if (core_.ServerCommand(command).first != 0) Assert::Fail();

			// Lists lobby
			command = "/lobby list " + lobbyTestName_;
			if (core_.ServerCommand(command).first != 0) Assert::Fail();

			// Start lobby
			command = "/lobby " + lobbyTestName_ + " start";
			if (core_.ServerCommand(command).first != 0) Assert::Fail();

			// Pause lobby
			command = "/lobby " + lobbyTestName_ + " pause";
			if (core_.ServerCommand(command).first != 0) Assert::Fail();

			// Delete lobby
			command = "/lobby " + lobbyTestName_ + " drop";
			Assert::IsTrue(core_.ServerCommand(command).first == 0);
		}
	};

}
