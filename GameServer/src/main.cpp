#include "Core.h"

int main() {
	Core core;
	std::thread t(&Core::Interpreter, &core);
	t.detach();
	core.Execute();
	return 0;
}