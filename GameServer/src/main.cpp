#include "pch.h"
#include "Core.h"

int main() {
	Core core;

	// Create a console reading thread
	std::thread t(&Core::Interpreter, &core);
	t.detach();
	
	// Start server
	core.Execute();
	return 0;
}