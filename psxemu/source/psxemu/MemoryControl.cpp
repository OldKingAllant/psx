#include <psxemu/include/psxemu/SystemBus.hpp>

namespace psx {
	void SystemBus::WriteMemControl(u32 address, u32 value) {
		if (address >= memory::IO::BIOS_CONFIG_CONTROL && address < 
			memory::IO::BIOS_CONFIG_CONTROL + 4) {
			ReconfigureBIOS(value);
			return;
		}

		if (address >= memory::IO::COM_DELAY && address <
			memory::IO::COM_DELAY + 4) {
			WriteCOMDelay(value);
			return;
		}

		if (address >= memory::IO::EXP1_BASE && address <
			memory::IO::EXP1_BASE + 4) {
			value &= 0x00FFFFFF;
			value |= 0x1F000000;
			u32 sz = (1 << m_exp1_config.delay_size.size_shift);
			m_exp1_config.base = value & ~(sz - 1);
			m_exp1_config.end = m_exp1_config.base + sz;
			fmt::println("EXP1 Start = 0x{:x}, End = 0x{:x}",
				m_exp1_config.base, m_exp1_config.end);
			return;
		}


		if (address >= memory::IO::EXP2_BASE && address <
			memory::IO::EXP2_BASE + 4) {
			if (value != memory::region_offsets::PSX_EXPANSION2_OFFSET) {
				m_exp2_enable = false;
				fmt::println("EXP2 Disabled!");
			}
			else {
				m_exp2_enable = true;
				fmt::println("EXP2 Enabled!");
			}

			return;
		}

		if (address >= memory::IO::EXP1_CONFIG && address <
			memory::IO::EXP1_CONFIG + 4) {
			fmt::println("EXP1 Reconfigured");
			WriteConf(m_exp1_config, value);
			return;
		}

		if (address >= memory::IO::EXP2_CONFIG && address <
			memory::IO::EXP2_CONFIG + 4) {
			fmt::println("EXP2 Reconfigured");
			WriteConf(m_exp2_config, value);
			return;
		}

		if (address >= memory::IO::EXP3_CONFIG && address <
			memory::IO::EXP3_CONFIG + 4) {
			fmt::println("EXP3 Reconfigured");
			WriteConf(m_exp3_config, value);
			return;
		}

#ifdef DEBUG_IO
		fmt::println("Write to invalid/unused/unimplemented mem control 0x{:x}", address);
#endif // DEBUG_IO
	}

	void SystemBus::WriteConf(RegionConfig& conf, u32 value) {
		if (conf.delay_size.raw == value)
			return;

		conf.delay_size.raw = value;

		conf.end = conf.base +
			(1 << conf.delay_size.size_shift);

		ComputeDelays(conf);
	}

	void SystemBus::WriteCOMDelay(u32 value) {
		if (m_com_delays.raw == value)
			return;

		m_com_delays.raw = value;

		fmt::println("COM Delay updated, COM0 = {}, COM1 = {}, COM2 = {}, COM3 = {}",
		(u32)m_com_delays.com0, (u32)m_com_delays.com1,
		(u32)m_com_delays.com2, (u32)m_com_delays.com3);

		ComputeDelays(m_bios_config);
		ComputeDelays(m_exp1_config);
		ComputeDelays(m_exp2_config);
		ComputeDelays(m_exp3_config);
	}

	void SystemBus::WriteCacheControl(u32 value) {
		if (m_cache_control.raw == value)
			return;

		bool old_scratchpad_en = m_cache_control.scratch_en1
			&& m_cache_control.scratch_en2;

		m_cache_control.raw = value;

		bool new_scratchpad_en = m_cache_control.scratch_en1
			&& m_cache_control.scratch_en2;

		if (old_scratchpad_en && !new_scratchpad_en) {
			fmt::println("SCRATCHPAD Disabled");
			ScratchpadDisable();
		}
		else if (!old_scratchpad_en && new_scratchpad_en) {
			fmt::println("SCRATCHPAD Enabled");
			ScratchpadEnable();
		}

		if (m_cache_control.cache_en) {
			fmt::println("Instruction cache enabled");
		}
	}

	u32 SystemBus::ReadMemControl(u32 address) const {
		if (address >= memory::IO::BIOS_CONFIG_CONTROL && address <
			memory::IO::BIOS_CONFIG_CONTROL + 4) {
			return m_bios_config.delay_size.raw;
		}

		if (address >= memory::IO::COM_DELAY && address <
			memory::IO::COM_DELAY + 4) {
			return m_com_delays.raw;
		}

		if (address >= memory::IO::EXP1_BASE && address <
			memory::IO::EXP1_BASE + 4) {
			return m_exp1_config.base;
		}


		if (address >= memory::IO::EXP2_BASE && address <
			memory::IO::EXP2_BASE + 4) {
			return m_exp2_config.base;
		}

		if (address >= memory::IO::EXP1_CONFIG && address <
			memory::IO::EXP1_CONFIG + 4) {
			return m_exp1_config.delay_size.raw;
		}

		if (address >= memory::IO::EXP2_CONFIG && address <
			memory::IO::EXP2_CONFIG + 4) {
			return m_exp2_config.delay_size.raw;
		}

		if (address >= memory::IO::EXP3_CONFIG && address <
			memory::IO::EXP3_CONFIG + 4) {
			return m_exp3_config.delay_size.raw;
		}

#ifdef DEBUG_IO
		fmt::println("Reading invalid/unused/unimplemented mem control 0x{:x}", address);
#endif // DEBUG_IO

		return 0x0;
	}

	u32 SystemBus::ReadCacheControl() const {
		return m_cache_control.raw;
	}
}