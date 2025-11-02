#include <psxemu/include/psxemu/SPU.hpp>

#include <psxemu/include/psxemu/SystemStatus.hpp>

#include <psxemu/include/psxemu/Logger.hpp>
#include <psxemu/include/psxemu/LoggerMacros.hpp>

namespace psx {
	SPU::SPU(system_status* sys_status) :
		m_sys_status{ sys_status }, 
		m_regs{}  {}

	u8 SPU::Read8(u32 address) {
		return 0x0;
	}

	u16 SPU::Read16(u32 address) {
		return 0x0;
	}

	u32 SPU::Read32(u32 address) {
		return 0x0;
	}

#pragma optimize("", off)
	void SPU::Write8(u32 address, u8 value) {
		LOG_ERROR("SPU", "[SPU] WRITE 8 {:x}", address + memory::IO::SPU_START);
		Write16(address, sign_extend<i16, 7>(value));
	}

	void SPU::Write16(u32 address, u16 value) {
		LOG_ERROR("SPU", "[SPU] WRITE 16 {:x}", address + memory::IO::SPU_START);

	}

	void SPU::Write32(u32 address, u32 value) {
		LOG_ERROR("SPU", "[SPU] WRITE 32 {:x}", address + memory::IO::SPU_START);
	}
#pragma optimize("", on)
}