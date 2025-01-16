#include <psxemu/include/psxemu/CDDrive.hpp>

#include <common/Errors.hpp>

#include <psxemu/include/psxemu/Logger.hpp>
#include <psxemu/include/psxemu/LoggerMacros.hpp>

#include <array>
#include <fmt/format.h>

namespace psx {
	void CDDrive::Command_GetStat() {
		m_stat.motor_on = m_motor_on;
		
		LOG_DEBUG("CDROM", "[CDROM] GetStat() -> {:#x}",
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

		LOG_DEBUG("CDROM", "[CDROM] GetID() -> INT3({:#x}) -> INT5({:#x})", 
			m_stat.reg, fmt::join(fixed, ","));

		PushResponse(CdInterrupt::INT3_FIRST_RESPONSE,
			{ m_stat.reg }, 0);
		PushResponse(CdInterrupt::INT5_ERR,
			{ 
				fixed[0], fixed[1], fixed[2], fixed[3], 
				fixed[4], fixed[5], fixed[6], fixed[7]
			}, ResponseTimings::GET_ID);
	}

	void CDDrive::Command_Setmode() {
		if (!ValidateParams(1))
			return;

		u8 new_mode = m_param_fifo.deque();

		m_mode.reg = new_mode;

		if (m_mode.ignore) {
			LOG_ERROR("CDROM", "[CDROM] Setmode() has ignore bit set!");
			error::DebugBreak();
		}

		PushResponse(CdInterrupt::INT3_FIRST_RESPONSE, { m_stat.reg },
			ResponseTimings::GETSTAT_NORMAL);

		LOG_DEBUG("CDROM", "[CDROM] Setmode(mode={:#x}) -> INT3({:#x})",
			new_mode, m_stat.reg);
		LOG_DEBUG("CDROM", "        Motor double speed    = {}", bool(m_mode.double_speed));
		LOG_DEBUG("CDROM", "        Enable XA ADPCM       = {}", bool(m_mode.enable_xa_adpcm));
		LOG_DEBUG("CDROM", "        Sector size           = {:#x}", m_mode.read_whole_sector ? 0x924 : 0x800);
		LOG_DEBUG("CDROM", "        Ignore sector size    = {}", bool(m_mode.ignore));
		LOG_DEBUG("CDROM", "        XA Filter enable      = {}", bool(m_mode.xa_filter_enable));
		LOG_DEBUG("CDROM", "        Report Interrupts     = {}", bool(m_mode.report));
		LOG_DEBUG("CDROM", "        Pause at end of track = {}", bool(m_mode.autopause));
		LOG_DEBUG("CDROM", "        Enable read CD-DA     = {}", bool(m_mode.allow_cd_da));
	}

	void CDDrive::Command_Stop() {
		m_stat.reading = false;
		u8 first_stat = m_stat.reg;
		m_stat.motor_on = false;
		u8 second_stat = m_stat.reg;

		LOG_DEBUG("CDROM", "[CDROM] Stop() -> INT3({:#x}) -> INT2({:#x})",
			first_stat, second_stat);

		PushResponse(CdInterrupt::INT3_FIRST_RESPONSE, { first_stat },
			ResponseTimings::GETSTAT_NORMAL);
		PushResponse(CdInterrupt::INT2_SECOND_RESPONSE, { second_stat },
			ResponseTimings::STOP);
	}
}