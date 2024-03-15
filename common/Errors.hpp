#pragma once

namespace psx::error {
	[[noreturn]] void DebugBreak();

	static void Unreachable() {
		__assume(false);
	}
}