#include "ShellSystemInterface.h"

#ifdef __APPLE__
#include <SDL/SDL.h>
#else
#include <SDL.h>
#endif

#ifdef _WIN32
#include <windows.h>
#endif

#include "sw_system.h"

// Get the number of seconds elapsed since the start of the application
float ShellSystemInterface::GetElapsedTime() {
	return 0.001f*Sys_GetTicks();
}

// Log the specified message.
// returns true to continue execution, false to break into the debugger.

bool ShellSystemInterface::LogMessage(Rocket::Core::Log::Type type, const Rocket::Core::String& message) {
#ifdef _WIN32
	OutputDebugStringA(message.CString());
	OutputDebugStringA("\n");
#endif
	printf("[ROCK] %s\n", message.CString());
	return true;
}
