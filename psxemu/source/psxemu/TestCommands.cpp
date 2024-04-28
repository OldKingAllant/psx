#include <psxemu/include/psxemu/CDDrive.hpp>

#include <common/Errors.hpp>

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
		fmt::println("[CDROM] Get BIOS date and version");

		std::array date_and_ver = { u8(0x95), u8(0x07), u8(0x24), u8(0xD1) };

		PushResponse(CdInterrupt::INT3_FIRST_RESPONSE,
			{ date_and_ver[0], date_and_ver[1], 
			  date_and_ver[2], date_and_ver[3] }, 0);
	}
}