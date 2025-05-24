#include <psxemu/include/psxemu/CDDrive.hpp>

#include <common/Errors.hpp>

#include <psxemu/include/psxemu/Logger.hpp>
#include <psxemu/include/psxemu/LoggerMacros.hpp>

#include <psxemu/include/psxemu/SystemStatus.hpp>

#include <array>
#include <fmt/format.h>

namespace psx {
	void CDDrive::Command_GetStat() {
		m_stat.motor_on = m_motor_on;
		
		LOG_DEBUG("CDROM", "[CDROM] GetStat() -> {:#x}",
			m_stat.reg);

		PushResponse(CdInterrupt::INT3_FIRST_RESPONSE,
			{ m_stat.reg }, ResponseTimings::GETSTAT_NORMAL);

		m_stat.shell_open = m_lid_open;
	}

	void CDDrive::Command_GetID() {
		m_stat.motor_on = m_motor_on;

		std::array<u8, 8> response{};

		if (m_cdrom) {
			auto const& region = GetConsoleRegion();

			const char* region_string = "    ";

			if (region == "AMERICA") {
				region_string = "SCEA";
			}
			else if (region == "EUROPE") {
				region_string = "SCEE";
			}
			else if (region == "JAPAN") {
				region_string = "SCEI";
			}

			response = {
				0x02, 0x00, 0x20, 0x00,
				u8(region_string[0]), u8(region_string[1]), 
				u8(region_string[2]), u8(region_string[3])
			};

			LOG_DEBUG("CDROM", "[CDROM] GetID() -> INT3({:#x}) -> INT2({:#x})",
				m_stat.reg, fmt::join(response, ","));
		}
		else {
			response = {
				0x08, 0x40, 0x00, 0x00,
				0x00, 0x00, 0x00, 0x00
			};

			LOG_DEBUG("CDROM", "[CDROM] GetID() -> INT3({:#x}) -> INT5({:#x})",
				m_stat.reg, fmt::join(response, ","));
		}

		PushResponse(CdInterrupt::INT3_FIRST_RESPONSE,
			{ m_stat.reg }, 0);
		PushResponse(m_cdrom ? CdInterrupt::INT2_SECOND_RESPONSE : CdInterrupt::INT5_ERR,
			{ 
				response[0], response[1], response[2], response[3],
				response[4], response[5], response[6], response[7]
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
		m_sys_status->scheduler.Deschedule(m_read_event);

		m_seek_loc = {};
		m_read_paused = false;

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

	void CDDrive::Command_ReadTOC() {
		m_stat.motor_on = m_motor_on;
		
		LOG_DEBUG("CDROM", "[CDROM] ReadTOC() -> INT3({:#x}) -> INT2({:#x})",
			m_stat.reg, m_stat.reg);

		PushResponse(CdInterrupt::INT3_FIRST_RESPONSE, { m_stat.reg },
			ResponseTimings::GETSTAT_NORMAL);
		PushResponse(CdInterrupt::INT2_SECOND_RESPONSE, { m_stat.reg },
			ResponseTimings::READ_TOC);
	}

	void CDDrive::Command_SetLoc() {
		if (!ValidateParams(3))
			return;

		m_stat.motor_on = m_motor_on;

		u8 amm = m_param_fifo.deque();
		u8 ass = m_param_fifo.deque();
		u8 asect = m_param_fifo.deque();

		m_unprocessed_seek_loc.mm = amm;
		m_unprocessed_seek_loc.ss = ass;
		m_unprocessed_seek_loc.sect = asect;
		m_has_unprocessed_seek = true;

		PushResponse(CdInterrupt::INT3_FIRST_RESPONSE, { m_stat.reg },
			ResponseTimings::GETSTAT_NORMAL);

		LOG_DEBUG("CDROM", "[CDROM] SetLoc(mm={:#x}, ss={:#x}, sect={:#x}) -> INT3({:#x})",
			amm, ass, asect, m_stat.reg);
	}

	void CDDrive::Command_SeekL() {
		m_stat.motor_on = m_motor_on;

		if (m_has_unprocessed_seek) {
			m_has_unprocessed_seek = false;
			m_seek_loc = m_unprocessed_seek_loc;
		}

		m_stat.playing = false;
		m_stat.seeking = true;
		m_stat.reading = false;
		u8 first_stat = m_stat.reg;
		m_stat.seeking = false;
		u8 second_stat = m_stat.reg;

		PushResponse(CdInterrupt::INT3_FIRST_RESPONSE, { first_stat },
			ResponseTimings::GETSTAT_NORMAL);
		PushResponse(CdInterrupt::INT2_SECOND_RESPONSE, { second_stat },
			ResponseTimings::GETSTAT_NORMAL);

		LOG_DEBUG("CDROM", "[CDROM] SeekL() -> INT3({:#x}) -> INT2({:#x})", 
			first_stat, second_stat);
	}

	extern void read_callback(void* userdata, u64 cycles_late);

	void CDDrive::Command_ReadN() {
		m_stat.motor_on = m_motor_on;

		if (m_has_unprocessed_seek) {
			m_has_unprocessed_seek = false;
			m_seek_loc = m_unprocessed_seek_loc;
			m_stat.seeking = true;
		}
		else {
			m_stat.reading = true;
		}

		PushResponse(CdInterrupt::INT3_FIRST_RESPONSE, { m_stat.reg },
			ResponseTimings::GETSTAT_NORMAL);

		m_read_event = m_sys_status->scheduler.Schedule(
			ResponseTimings::GETSTAT_NORMAL + ResponseTimings::READ,
			read_callback, std::bit_cast<void*>(this));

		LOG_DEBUG("CDROM", "[CDROM] ReadN() -> INT3({:#x})",
			m_stat.reg);
	}
}