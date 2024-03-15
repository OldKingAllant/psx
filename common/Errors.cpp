#include "Errors.hpp"

namespace psx::error {
	void DebugBreak() {
		__debugbreak();
	}
}