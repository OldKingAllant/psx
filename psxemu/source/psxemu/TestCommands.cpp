#include <psxemu/include/psxemu/CDDrive.hpp>
#include <psxemu/include/psxemu/SystemStatus.hpp>

#include <common/Errors.hpp>

#include <psxemu/include/psxemu/Logger.hpp>
#include <psxemu/include/psxemu/LoggerMacros.hpp>

#include <array>
#include <fmt/format.h>

namespace psx {
	enum class TestCommand {
		GET_BIOS_BCD_DATE_VERSION = 0x20
	};

	void CDDrive::CommandTest(u8 cmd) {
		if (!ValidateParams(1)) {
			return;
		}

		TestCommand test_cmd = TestCommand(m_param_fifo.deque());

		switch (test_cmd)
		{
		case psx::TestCommand::GET_BIOS_BCD_DATE_VERSION:
			Command_GetBiosBCD();
			break;
		default:
			error::DebugBreak();
			break;
		}

		m_idle = true;
	}

	void CDDrive::Command_GetBiosBCD() {
		LOG_DEBUG("CDROM", "[CDROM] Get BIOS date and version");

		std::array date_and_ver = { u8(0x95), u8(0x07), u8(0x24), u8(0xD1) };

		PushResponse(CdInterrupt::INT3_FIRST_RESPONSE,
			{ date_and_ver[0], date_and_ver[1], 
			  date_and_ver[2], date_and_ver[3] }, 0);

		if (m_keep_history) {
			DriveCommand cmd{};

			cmd.command_id = 0x1A;
			cmd.command_name = "Test::GetBiosDateAndVersion()";
			cmd.params[0] = u8(TestCommand::GET_BIOS_BCD_DATE_VERSION);
			cmd.num_params = 1;
			cmd.num_responses = 1;
			cmd.responses[0] = m_response_fifo.back();
			cmd.issue_timestamp = m_sys_status->scheduler.GetTimestamp();

			m_history.push_back(cmd);
		}
	}
}