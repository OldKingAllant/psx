#include <psxemu/include/psxemu/CDDrive.hpp>

#include <fmt/format.h>
#include <array>

namespace psx {
	void CDDrive::Command_GetStat() {
		m_stat.motor_on = m_motor_on;
		
		fmt::println("[CDROM] GetStat() -> {:#x}",
			m_stat.reg);

		PushResponse(CdInterrupt::INT3_FIRST_RESPONSE,
			{ m_stat.reg }, ResponseTimings::GETSTAT_NORMAL);

		m_stat.shell_open = false;
	}

	void CDDrive::Command_GetID() {
		m_stat.motor_on = m_motor_on;

		std::array<u8, 8> fixed = { 
			0x08, 0x40, 0x00, 0x00,  
			0x00, 0x00, 0x00, 0x00
		};

		fmt::println("[CDROM] GetID() -> INT3({:#x}) -> INT5({:#x})", 
			m_stat.reg, fmt::join(fixed, ","));

		PushResponse(CdInterrupt::INT3_FIRST_RESPONSE,
			{ m_stat.reg }, 0);
		PushResponse(CdInterrupt::INT5_ERR,
			{ 
				fixed[0], fixed[1], fixed[2], fixed[3], 
				fixed[4], fixed[5], fixed[6], fixed[7]
			}, ResponseTimings::GET_ID);
	}
}