#include "pch.h"
#include "Core.h"
#include "SharedMemory.h"

/**
    main.cpp
    Purpose: Main file. Creates a core class and an interpreter

    @author Hampus Hallkvist
    @version 0.5 08/04/2019
*/

int main() {

	Core core;

	// Create a console reading thread
	std::thread t(&Core::Interpreter, &core);
	t.detach();
	
	// Start server
	core.Execute();
	return 0;
}