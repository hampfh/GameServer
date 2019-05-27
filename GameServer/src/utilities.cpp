#include "pch.h"
#include "utilities.h"


bool hgs::utilities::IsInt(std::string& string) {
	try {
		std::stoi(string);
		return true;
	}
	catch (...) { return false; }
}		
