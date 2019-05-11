#include "pch.h"
#include "Core.h"

/**
    main.cpp
    Purpose: Main file. Creates a core class and an interpreter

    @author Hampus Hallkvist
    @version 0.5 08/04/2019
*/

int main(int argc, char* argv[]) {

	Core core;

	if (!core.ready)
		return 1;

	// Create a console reading thread
	std::thread commandThread(&Core::ConsoleThread, &core);
	commandThread.detach();
	
	// Start server
	core.Execute();
	return 0;
}