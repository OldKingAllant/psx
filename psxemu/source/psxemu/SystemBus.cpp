#include <psxemu/include/psxemu/SystemBus.hpp>
#include <psxemu/include/psxemu/MemoryMapper.hpp>

#include <optional>

#include <fmt/format.h>

#include <psxemu/include/psxemu/Logger.hpp>
#include <psxemu/include/psxemu/LoggerMacros.hpp>

namespace psx {
	SystemBus::SystemBus(system_status* sys_status) :
		m_sys_status(sys_status),
		m_mapper(new memory::MemoryMapper(std::nullopt)),
		m_guest_base{ nullptr }, m_curr_ram_sz{}, m_ram_end{},
		m_ram_config{}, m_cache_control{}, m_bios_config{},
		m_exp1_config{}, m_exp2_config{}, m_exp3_config{},
		m_spu_config{}, m_cdrom_config{},
		m_exp2_enable{ true }, m_com_delays{}, m_curr_cycles{},
		m_count1{ 0, sys_status }, m_count2{ 1, sys_status }, 
		m_count3{ 2, sys_status }, m_dma_controller{sys_status}, 
		m_gpu{ sys_status }, m_cdrom{ sys_status }, 
		m_sio0{ sys_status, 0 }, m_sio1{ sys_status, 1 } {
		m_bios_config.base = memory::region_offsets::PSX_BIOS_OFFSET;
		m_exp1_config.base = memory::region_offsets::PSX_EXPANSIONS1_OFFSET;
		m_exp2_config.base = memory::region_offsets::PSX_EXPANSION2_OFFSET;
		m_exp3_config.base = memory::region_offsets::PSX_EXPANSION3_OFFSET;

		m_bios_config.delay_size.raw = BIOS_CONFIG_INIT;
		m_exp1_config.delay_size.raw = EXP1_CONFIG_INIT;
		m_exp2_config.delay_size.raw = EXP2_CONFIG_INIT;
		m_exp3_config.delay_size.raw = EXP3_CONFIG_INIT;

		m_bios_config.end = m_bios_config.base +
			(1 << m_bios_config.delay_size.size_shift);
		m_exp1_config.end = m_exp1_config.base +
			(1 << m_exp1_config.delay_size.size_shift);
		m_exp2_config.end = m_exp2_config.base +
			(1 << m_exp2_config.delay_size.size_shift);
		m_exp3_config.end = m_exp3_config.base +
			(1 << m_exp3_config.delay_size.size_shift);

		m_ram_config = RAM_SIZE_INIT;

		InitMapRegions();
		m_guest_base = m_mapper->GetGuestBase();

		ComputeDelays(m_bios_config);

		LOG_WARN("MEMORY", "[MEMORY] Guest base = 0x{:x}\n", std::bit_cast<u64>(m_guest_base));
		LOG_WARN("MEMORY", "[MEMORY] BIOS Base = 0x{:x}, End = 0x{:x}, Size = {}\n",
			m_bios_config.base, m_bios_config.end, m_bios_config.end - m_bios_config.base);
		LOG_WARN("MEMORY", "[MEMORY] EXP1 Base = 0x{:x}, End = 0x{:x}, Size = {}\n",
			m_exp1_config.base, m_exp1_config.end, m_exp1_config.base - m_exp1_config.end);
		LOG_WARN("MEMORY", "[MEMORY] EXP2 Base = 0x{:x}, End = 0x{:x}, Size = {}\n",
			m_exp2_config.base, m_exp2_config.end, m_exp2_config.base - m_exp2_config.end);
		LOG_WARN("MEMORY", "[MEMORY] EXP2 Base = 0x{:x}, End = 0x{:x}, Size = {}\n",
			m_exp3_config.base, m_exp3_config.end, m_exp3_config.base - m_exp3_config.end);
	}

	bool SystemBus::MapRam(u64 size_mb) {
		if (size_mb != 1 && size_mb != 2 &&
			size_mb != 4 && size_mb != 8)
			return false;

		u32 curr_segment = 0;

		constexpr u64 segment_offs[] = {
			memory::KUSEG_START,
			memory::KSEG0_START,
			memory::KSEG1_START
		};

		u64 size_step = 2;

		if (size_mb == 1)
			size_step = 1;

		constexpr u64 MB = (u64)1024 * 1024;

		while (curr_segment < 3) {
			u64 mapped_sz = 0;

			while (mapped_sz < size_mb) {
				u64 offset = segment_offs[curr_segment] + mapped_sz * MB;

				RETURN_IF_FALSE(m_mapper->ReserveRegion(offset,
					MB * size_step), false);
				RETURN_IF_EQ(m_mapper->MapRegion(offset,
					MB * size_step,
					memory::region_mappings::PSX_MAIN_RAM_MAPPING,
					memory::PageProtection::READ_WRITE), nullptr, false);

				mapped_sz += size_step;
			}

			curr_segment++;
		}

		return true;
	}

	bool SystemBus::ResetRamMap() {
		u32 curr_segment = 0;

		constexpr u64 segment_offs[] = {
			memory::KUSEG_START,
			memory::KSEG0_START,
			memory::KSEG1_START
		};

		u64 size_step = 2;
		u64 total_map = 0;

		switch (m_curr_ram_sz)
		{
		case psx::RamSize::_1MB_7MB:
		case psx::RamSize::_1MB_1HIGHZ_6MB:
			size_step = 1;
			total_map = 1;
			break;
		case psx::RamSize::_4MB_4MB:
		case psx::RamSize::_4MB_4HIGHZ:
			total_map = 4;
			break;
		case psx::RamSize::_2MB_6MB:
		case psx::RamSize::_2MB_2HIGHZ_4MB:
			total_map = 2;
			break;
		case psx::RamSize::_8MB1:
		case psx::RamSize::_8MB2:
			total_map = 8;
			break;
		default:
			return false;
			break;
		}
		
		u8* guest_base = m_mapper->GetGuestBase();

		constexpr u64 MB = (u64)1024 * 1024;

		//Unmap all views and free all the regions
		//in a single go
		while (curr_segment < 3) {
			u64 unmapped_sz = 0;

			while (unmapped_sz < total_map) {
				u64 curr_offset = segment_offs[curr_segment] +
					unmapped_sz * MB;
				u8* curr_base = guest_base +
					segment_offs[curr_segment] +
					unmapped_sz * MB;

				RETURN_IF_FALSE(m_mapper->UnmapRegion(curr_base), false);
				RETURN_IF_FALSE(m_mapper->FreeRegion(curr_offset), false);

				unmapped_sz += size_step;
			}

			curr_segment++;
		}


		return true;
	}

	bool SystemBus::SetRamMap(RamSize new_config) {
		m_curr_ram_sz = new_config;

		constexpr u64 MB = (u64)1024 * 1024;

		switch (new_config)
		{
		case psx::RamSize::_1MB_7MB:
		case psx::RamSize::_1MB_1HIGHZ_6MB:
			m_ram_end = MB;
			return MapRam(1);
			break;
		case psx::RamSize::_4MB_4MB:
		case psx::RamSize::_4MB_4HIGHZ:
			m_ram_end = MB * 4;
			return MapRam(4);
			break;
		case psx::RamSize::_2MB_6MB:
		case psx::RamSize::_2MB_2HIGHZ_4MB:
			m_ram_end = MB * 2;
			return MapRam(2);
			break;
		case psx::RamSize::_8MB1:
		case psx::RamSize::_8MB2:
			m_ram_end = MB * 8;
			return MapRam(8);
			break;
		default:
			break;
		}

		return false;
	}

	bool SystemBus::ScratchpadEnable() {
		u32 curr_segment = 0;

		constexpr u64 segment_offs[] = {
			memory::KUSEG_START,
			memory::KSEG0_START,
			memory::KSEG1_START
		};

		while (curr_segment < 2) {
			u64 curr_offset = segment_offs[curr_segment] +
				memory::region_offsets::PSX_SCRATCHPAD_OFFSET;
			u64 map_size = memory::region_sizes::PSX_SCRATCHPAD_PADDED_SIZE;
			u64 file_off = memory::region_mappings::PSX_SCRATCHPAD_MAPPING;

			RETURN_IF_FALSE(m_mapper->ReserveRegion(curr_offset, map_size), false);
			RETURN_IF_EQ(m_mapper->MapRegion(curr_offset, map_size, file_off,
				memory::PageProtection::READ_WRITE), nullptr, false);

			curr_segment++;
		}

		return true;
	}

	bool SystemBus::ScratchpadDisable() {
		u32 curr_segment = 0;

		constexpr u64 segment_offs[] = {
			memory::KUSEG_START,
			memory::KSEG0_START,
			memory::KSEG1_START
		};

		auto guest_base = m_mapper->GetGuestBase();

		while (curr_segment < 2) {
			u64 curr_offset = segment_offs[curr_segment] +
				memory::region_offsets::PSX_SCRATCHPAD_OFFSET;
			u8* curr_base = guest_base + curr_offset;

			RETURN_IF_FALSE(m_mapper->UnmapRegion(curr_base), false);
			RETURN_IF_FALSE(m_mapper->FreeRegion(curr_offset), false);

			curr_segment++;
		}

		return true;
	}

	bool SystemBus::SetBiosMap(u32 new_size, bool read_only) {
		u32 curr_segment = 0;

		u32 real_bios_sz = memory::region_sizes::PSX_BIOS_SIZE;

		new_size = (u32)UpAlignTo(new_size, m_mapper->GetPageGranularity());

		u32 total_maps_per_segment = new_size / real_bios_sz;

		if (new_size % real_bios_sz != 0)
			total_maps_per_segment += 1;

		constexpr u64 segment_offs[] = {
			memory::KUSEG_START,
			memory::KSEG0_START,
			memory::KSEG1_START
		};

		auto protect = read_only ? memory::PageProtection::READ :
			memory::PageProtection::READ_WRITE;

		while (curr_segment < 3) {
			u64 curr_offset = segment_offs[curr_segment] +
				memory::region_offsets::PSX_BIOS_OFFSET;
			u64 file_off = memory::region_mappings::PSX_BIOS_MAPPING;
			u32 remaining_maps = total_maps_per_segment;

			while (remaining_maps) {
				RETURN_IF_FALSE(m_mapper->ReserveRegion(curr_offset, real_bios_sz), false);
				RETURN_IF_EQ(m_mapper->MapRegion(curr_offset, real_bios_sz,
					file_off, protect), nullptr, false);
				remaining_maps--;
				curr_offset += real_bios_sz;
			}

			curr_segment++;
		}

		return true;
	}

	bool SystemBus::ResetBiosMap() {
		u32 curr_segment = 0;

		u32 real_bios_sz = memory::region_sizes::PSX_BIOS_SIZE;
		u32 size_shift = m_bios_config.delay_size.size_shift;

		u32 map_size = 1 << size_shift;
		map_size = (u32)UpAlignTo(map_size, m_mapper->GetPageGranularity());

		u32 total_maps_per_segment = map_size / real_bios_sz;

		if (map_size % real_bios_sz != 0)
			total_maps_per_segment += 1;

		constexpr u64 segment_offs[] = {
			memory::KUSEG_START,
			memory::KSEG0_START,
			memory::KSEG1_START
		};

		auto guest_base = m_mapper->GetGuestBase();

		while (curr_segment < 3) {
			u64 curr_offset = segment_offs[curr_segment] +
				memory::region_offsets::PSX_BIOS_OFFSET;
			u8* curr_base = guest_base + curr_offset;
			u32 curr_maps = total_maps_per_segment;

			while (curr_maps) {
				RETURN_IF_FALSE(m_mapper->UnmapRegion(curr_base), false);
				RETURN_IF_FALSE(m_mapper->FreeRegion(curr_offset), false);
				curr_maps--;
				curr_offset += real_bios_sz;
				curr_base += real_bios_sz;
			}

			curr_segment++;
		}

		return true;
	}

	void SystemBus::InitMapRegions() {
		constexpr u64 NORMAL_BIOS_SIZE = (u64)512 * 1024;


		LOG_WARN("MEMORY", "[MEMORY] Mapping RAM to 8MB\n");
		// Map RAM first
		THROW_IF_FALSE(SetRamMap(RamSize::_8MB1), std::exception("RAM map failed"));

		LOG_WARN("MEMORY", "[MEMORY] Mapping SCRATCHPAD\n");
		THROW_IF_FALSE(ScratchpadEnable(), std::exception("SCRATCHPAD map failed"));

		LOG_WARN("MEMORY", "[MEMORY] Mapping BIOS to 512KB\n");
		THROW_IF_FALSE(SetBiosMap(NORMAL_BIOS_SIZE, false), std::exception("BIOS map failed"));
	}

	void SystemBus::CopyRaw(u8 const* ptr, u32 dest, u32 size) {
		//TODO Invalidate JIT blocks
		std::memcpy(m_guest_base + dest, ptr, size);
	}

	void SystemBus::ReadRaw(u8* dst, u32 src, u32 size) {
		std::memcpy(dst, m_guest_base + src, size);
	}

	void SystemBus::LoadBios(u8* data, u32 size) {
		constexpr u64 NORMAL_BIOS_SIZE = (u64)512 * 1024;

		ResetBiosMap();
		THROW_IF_FALSE(SetBiosMap(NORMAL_BIOS_SIZE, false), std::exception("BIOS map failed"));

		CopyRaw(data, memory::region_offsets::PSX_BIOS_OFFSET, size);

		THROW_IF_FALSE(ResetBiosMap(), std::exception("BIOS map failed"));
		THROW_IF_FALSE(SetBiosMap(NORMAL_BIOS_SIZE, true), std::exception("BIOS map failed"));
	}

	void SystemBus::ComputeDelays(RegionConfig& conf) {
		u32 load_delay = conf.delay_size.read_delay;
		u32 store_delay = conf.delay_size.write_delay;

		//Compute base delay
		u32 nonseq = 0, seq = 0, min = 0;

		if (conf.delay_size.use_com0) {
			nonseq = m_com_delays.com0 - 1;
			seq = m_com_delays.com0 - 1;
		}

		if (conf.delay_size.use_com2) {
			nonseq += m_com_delays.com2;
			seq += m_com_delays.com2;
		}

		if (conf.delay_size.use_com3)
			min = m_com_delays.com3;

		if (nonseq < 6)
			nonseq += 1;
		///////

		u32 nonseq_temp = nonseq + load_delay + 2;
		u32 seq_temp = seq + load_delay + 2;

		if (nonseq_temp < min + 6)
			nonseq_temp = min + 6;

		if (seq_temp < min + 2)
			seq_temp = min + 2;

		conf.read_nonseq = nonseq_temp;
		conf.read_seq = seq_temp;

		////////////////

		nonseq_temp = nonseq + store_delay + 2;
		seq_temp = seq + store_delay + 2;

		if (nonseq_temp < min + 6)
			nonseq_temp = min + 6;

		if (seq_temp < min + 2)
			seq_temp = min + 2;

		conf.write_nonseq = nonseq_temp;
		conf.write_seq = seq_temp;
	}

	void SystemBus::ReconfigureBIOS(u32 new_config) {
		if (m_bios_config.delay_size.raw == new_config) {
			LOG_DEBUG("MEMORY", "[MEMORY] BIOS already configured with value 0x{:#x}", new_config);
			return;
		}

		ResetBiosMap();
		m_bios_config.delay_size.raw = new_config;
		ComputeDelays(m_bios_config);

		u32 size = (1 << m_bios_config.delay_size.size_shift);

		m_bios_config.end = m_bios_config.base +
			(1 << m_bios_config.delay_size.size_shift);

		if (!SetBiosMap(size, true))
			LOG_ERROR("MEMORY", "[MEMORY] Failed BIOS remapping with size 0x{:#x}, better shut down the emulator...", 
				size);
		else
			LOG_WARN("MEMORY", "[MEMORY] BIOS reconfigured - Base = 0x{:#x}, End = 0x{:#x}, Size = {:#x}, Read delay = {}, Write delay = {}\n",
				m_bios_config.base, m_bios_config.end, m_bios_config.end - m_bios_config.base, 
				m_bios_config.read_nonseq, m_bios_config.write_nonseq);
	}

	void SystemBus::ReconfigureRAM(u32 ram_conf) {
		if (ram_conf == m_ram_config) {
			LOG_DEBUG("MEMORY", "[MEMORY] RAM already configured with value 0x{:#x}", ram_conf);
			return;
		}

		m_ram_config = ram_conf;

		u8 new_size = (ram_conf >> 9) & 0x7;

		ResetRamMap();

		if (!SetRamMap((RamSize)new_size))
			LOG_ERROR("MEMORY", "[MEMORY] RAM reconfiguration failed, this is a fatal error");
		else
			LOG_WARN("MEMORY", "[MEMORY] RAM reconfigured with value 0x{:#x}", ram_conf);
	}

	void SystemBus::WriteEXP1(u32 value, u32 address) {
		LOG_DEBUG("MEMORY", "[MEMORY] EXP1 write at 0x{:#x} = 0x{:#x}", address, value);
	}

	void SystemBus::WriteEXP2(u32 value, u32 address) {
		if (address == 0x41) {
			LOG_DEBUG("MEMORY", "[POST] Kernel trace 0x{:#x}", value);
			return;
		}

		LOG_DEBUG("MEMORY", "[MEMORY] EXP2 write at 0x{:x} = 0x{:x}", address, value);
	}

	void SystemBus::WriteEXP3(u32 value, u32 address) {
		LOG_DEBUG("MEMORY", "[MEMORY] EXP3 write at 0x{:#x} = 0x{:#x}", address, value);
	}

	u32 SystemBus::ReadEXP1(u32 address) {
		return 0x0;
	}

	u32 SystemBus::ReadEXP2(u32 address) {
		return 0x0;
	}

	u32 SystemBus::ReadEXP3(u32 address) {
		return 0x0;
	}

	SystemBus::~SystemBus() {
		if (m_mapper)
			delete m_mapper;
	}
}