#pragma once

#include <common/Macros.hpp>
#include <common/Defs.hpp>

#include <psxemu/include/psxemu/SystemStatus.hpp>

#include <fmt/format.h>

namespace psx::memory {
	class MemoryMapper;
}

namespace psx {
	enum class RamSize : u8 {
		_1MB_7MB,
		_4MB_4MB,
		_1MB_1HIGHZ_6MB,
		_4MB_4HIGHZ,
		_2MB_6MB,
		_8MB1,
		_2MB_2HIGHZ_4MB,
		_8MB2
	};

	union CacheControl {
#pragma pack(push, 1)
		struct {
			u8 : 3;
			bool scratch_en1 : 1;
			u8 : 3;
			bool scratch_en2 : 1;
			bool : 1;
			bool crash : 1;
			bool : 1;
			bool cache_en : 1;
		};
#pragma pack(pop)

		u32 raw;
	};

	/// <summary>
	/// Delay and size configuration of
	/// a region
	/// </summary>
	union DelaySizeConfig {
#pragma pack(push, 1)
		struct {
			u8 : 4;
			u8 access_time : 4;
			bool use_com0 : 1;
			bool use_com1 : 1;
			bool use_com2 : 1;
			bool use_com3 : 1;
			bool bus_width : 1;
			u8 : 3;
			u8 size_shift : 5;
		};
#pragma pack(pop)

		u32 raw;
	};

	struct RegionConfig {
		DelaySizeConfig delay_size;
		u32 base;
		u32 end;
	};

	/// <summary>
	/// Align to the next alignment 
	/// given by align. Beware! Does
	/// not check that align is, in fact,
	/// a power of two
	/// </summary>
	/// <param name="num">Num to align</param>
	/// <param name="align">Alignment</param>
	FORCE_INLINE static u64 UpAlignTo(u64 num, u64 align) {
		if (num & (align - 1))
			num = (num + align) & align;

		return num;
	}

	class SystemBus {
	public :
		SystemBus(system_status* sys_status);
		~SystemBus();

		/// <summary>
		/// Copy raw data inside the guest
		/// address space.
		/// </summary>
		/// <param name="ptr">Pointer to data to copy</param>
		/// <param name="dest">Destination offset</param>
		/// <param name="size">Size of write</param>
		void CopyRaw(u8 const* ptr, u32 dest, u32 size);

		/// <summary>
		/// Reads directly from the guest address
		/// space and writes to the given dest buffer
		/// </summary>
		/// <param name="dst">Dest buffer</param>
		/// <param name="src">Start address</param>
		/// <param name="size">Read size</param>
		void ReadRaw(u8* dst, u32 src, u32 size);

		//512 KB, 8 bit bus
		static constexpr u64 BIOS_CONFIG_INIT = 0x013243F;

		//512 KB, 8 bit bus
		static constexpr u64 EXP1_CONFIG_INIT = 0x013243F;

		//128 B, 8 bit bus
		static constexpr u64 EXP2_CONFIG_INIT = 0x0070777;

		//1 Byte
		static constexpr u64 EXP3_CONFIG_INIT = 0x0003022;

		static constexpr u64 KUSEG_VOID_START = (u64)512 * 1024 * 1024;
		static constexpr u64 KUSEG_VOID_END = (u64)2 * 1024 * 1024 * 1024;

		/// <summary>
		/// Emulate read (unless fastmem is applicable)
		/// </summary>
		/// <typeparam name="Ty">"Type" that corresponds to the size of the read</typeparam>
		/// <typeparam name="Except">Throw exceptions on invalid reads</typeparam>
		/// <param name="address">Read location</param>
		/// <returns>The bytes at "address"</returns>
		template <typename Ty, bool Except>
		Ty Read(u32 address) {
			if constexpr (sizeof(Ty) != 1) {
				if (address & (sizeof(Ty) - 1) != 0) {
					//Unaligned access!
#ifdef DEBUG
					fmt::print("Unaligned access at 0x{:x}\n", address);
#endif // DEBUG

					if constexpr (Except) {
						m_sys_status->exception = true;
						m_sys_status->exception_number =
							cpu::Excode::ADEL;
						m_sys_status->badvaddr = address;
					}

					return 0x0;
				}
			}

			bool curr_mode = m_sys_status->curr_mode;

			if (address >= KUSEG_VOID_START && address < KUSEG_VOID_END) {
#ifdef DEBUG
				fmt::print("Reading unused upper 1.5 GB of KUSEG at 0x{:x}\n", address);
#endif // DEBUG
				if constexpr (Except) {
					m_sys_status->exception = true;
					m_sys_status->exception_number =
						cpu::Excode::DBE;
					//Do not set BADVADDR
				}

				return 0x0;
			}

			if (address >= memory::KSEG2_START) {
				fmt::print("KSEG2 access at 0x{:x}\n", address);
				return 0x0;
			}

			u32 segment = (address >> 29) & 7;
			u32 lower = address & 0x1FFFFFFF;

			if (curr_mode && segment != 0) {
				//Reading KSEG in user mode
#ifdef DEBUG
				fmt::print("Reading KSEG in USER mode at 0x{:x}\n", address);
#endif // DEBUG
				if constexpr (Except) {
					m_sys_status->exception = true;
					m_sys_status->exception_number =
						cpu::Excode::ADEL;
					m_sys_status->badvaddr = address;
				}

				return 0x0;
			}

			if (lower < m_ram_end) {
				return *reinterpret_cast<Ty*>(m_guest_base + address);
			}

			if (lower >= memory::KSEG2_START) {
				return 0x0;
			}

			/*
			Test access to expansion regions
			and bios
			*/

			if (m_exp2_enable && lower >=
				m_exp2_config.base && lower < m_exp2_config.end) {
				return 0x0;
			}

			if (lower >= m_exp1_config.base && lower < m_exp1_config.end) {
				return 0x0;
			}

			if (lower >= m_exp3_config.base && lower < m_exp3_config.end) {
				return 0x0;
			}

			if (lower >= m_bios_config.base && lower < m_bios_config.end) {
				return *reinterpret_cast<Ty*>(m_guest_base + address);
			}

			if (lower >= memory::region_offsets::PSX_SCRATCHPAD_OFFSET
				&& lower < memory::region_offsets::PSX_SCRATCHPAD_OFFSET +
				memory::region_sizes::PSX_SCRATCHPAD_PADDED_SIZE) {
				if (segment == 0x5) {
#ifdef DEBUG
					fmt::print("Reading scratchpad in KSEG1 at 0x{:x}\n", address);
#endif // DEBUG
					if constexpr (Except) {
						m_sys_status->exception = true;
						m_sys_status->exception_number =
							cpu::Excode::DBE;
					}

					return 0x0;
				}

				return *reinterpret_cast<Ty*>(m_guest_base + address);
			}

			if (lower >= memory::region_offsets::PSX_IO_OFFSET
				&& lower < memory::region_offsets::PSX_IO_OFFSET +
				memory::region_sizes::PSX_IO_SIZE) {
				return 0x0;
			}

			if constexpr (Except) {
#ifdef DEBUG
				fmt::print("Reading unused memory at 0x{:x}\n", address);
#endif // DEBUG
				m_sys_status->exception = true;
				m_sys_status->exception_number =
					cpu::Excode::DBE;
			}

			return 0x0;
		}

	private :
		/// <summary>
		/// Called only once at the beginning,
		/// maps regions 
		/// </summary>
		void InitMapRegions();

		/// <summary>
		/// Performs unmap/coalesce of the
		/// RAM regions depending on the
		/// current configuration
		/// </summary>
		bool ResetRamMap();

		/// <summary>
		/// Set new RAM map configuration
		/// </summary>
		/// <param name="new_config">New RAM config</param>
		bool SetRamMap(RamSize new_config);

		/// <summary>
		/// Map RAM to all 3 segments with the given
		/// size in MB. Valid values are 1,2,4,8.
		/// Note that if size > 2MB than above 2MB
		/// are mirrors of the bottom 2MB
		/// </summary>
		/// <param name="size_mb">Mirror with size</param>
		bool MapRam(u64 size_mb);

		/*
		* Map/Unmap the SCRATCHPAD region
		* This operation should be performed
		* when some bits in cache control are
		* set to zero
		*/

		bool ScratchpadEnable();
		bool ScratchpadDisable();

		bool SetBiosMap(u32 new_size);
		bool ResetBiosMap();

	private :
		system_status* m_sys_status;

		memory::MemoryMapper* m_mapper;
		u8* m_guest_base;

		RamSize m_curr_ram_sz;
		u32 m_ram_end;
		CacheControl m_cache_control;
		RegionConfig m_bios_config;
		RegionConfig m_exp1_config;
		RegionConfig m_exp2_config;
		RegionConfig m_exp3_config;

		bool m_exp2_enable;
	};
}