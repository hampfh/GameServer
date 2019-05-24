# Multithreaded GameServer

## Features
* Manage client communication 
* Split client into different lobbies where they're not interupting each other
* Log all communication in the server console
* Log client communication
* Rcon - connect to server with third party software

 **Platform:** Windows 10

GameServer is a bridge application between different clients allowing for any communication wanted. Without any form of gamelogic the server only stores the clients commands and broadcast them to all other connected. Thus making the server highly flexible and possible to use for a vast spectre of different games. The server notify connected clients when something special appears such as a disconnect or connect from a new client. 

Thanks to multithreading every connection has it's own dedicated listener and sender, thus making everything fast and efficient. The external threads are threadsafe to prevent collisions when writing to memory. 

For more information about the functionallity and possibility to manage the server read the [Documentation](../../wiki)

## Usage of Third Party Libraries
* [Spdlog](https://github.com/gabime/spdlog)
* [SCL - Simple Config Library](https://github.com/WizardCarter/simple-config-library)
* [GSL - Guideline support library](https://github.com/microsoft/GSL)
