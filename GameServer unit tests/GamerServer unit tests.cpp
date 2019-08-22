#include "pch.h"
#include "CppUnitTest.h"
#include "../GameServer/src/core.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameServerunittests
{
	TEST_CLASS(GameServerCoreTests) {
	public:
		
		TEST_METHOD(TestCoreConstructor) {
			bool expected = true;
			hgs::Core core;
			Assert::AreEqual(expected, core.ready);
		}
	};
}
