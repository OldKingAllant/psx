#pragma once

#include <string>

namespace psx {
#ifdef _WIN32
	/// <summary>
	/// Set current thread's name
	/// </summary>
	/// <param name="name"></param>
	/// <returns>Success or failure</returns>
	bool SetThreadName(std::string const& name);
#else
#error "Unsopported platform"
#endif // _WIN32
}