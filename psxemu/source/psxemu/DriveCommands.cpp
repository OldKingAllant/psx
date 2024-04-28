#include <psxemu/include/psxemu/CDDrive.hpp>

#include <fmt/format.h>

namespace psx {
	void CDDrive::Command_GetStat() {
		m_stat.motor_on = m_motor_on;
		m_stat.shell_open = false;
		
		fmt::println("[CDROM] GetStat() -> {:#x}",
			m_stat.reg);

		PushResponse(CdInterrupt::INT3_FIRST_RESPONSE,
			{ m_stat.reg }, 0);
	}
}