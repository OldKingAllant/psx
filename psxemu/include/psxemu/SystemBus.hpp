#pragma once

#include <common/Macros.hpp>
#include <common/Defs.hpp>
#include <common/Errors.hpp>

#include <psxemu/include/psxemu/SystemStatus.hpp>
#include <psxemu/include/psxemu/IOGaps.hpp>
#include <psxemu/include/psxemu/RootCounters.hpp>
#include <psxemu/include/psxemu/DmaController.hpp>
#include <psxemu/include/psxemu/GPU.hpp>

#include <fmt/format.h>

class DebugView;

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
			u8 write_delay : 4;
			u8 read_delay : 4;
			bool use_com0 : 1;
			bool use_com1 : 1;
			bool use_com2 : 1;
			bool use_com3 : 1;
			bool bus_width : 1;
			bool auto_inc : 1;
			u8 : 2;
			u8 size_shift : 5;
			u8 : 3;
			u8 dma_timing_override : 4;
			bool address_error : 1;
			bool dma_timing_select : 1;
			bool wide_dma : 1;
			bool wait : 1;
		};
#pragma pack(pop)

		u32 raw;
	};

	struct RegionConfig {
		DelaySizeConfig delay_size;
		u32 base;
		u32 end;
		u32 read_nonseq;
		u32 read_seq;
		u32 write_nonseq;
		u32 write_seq;
	};

	/// <summary>
	/// Common delays for regions
	/// </summary>
	union ComDelay {
#pragma pack(push, 1)
		struct {
			u8 com0 : 4;
			u8 com1 : 4;
			u8 com2 : 4;
			u8 com3 : 4;
		};
#pragma pack(pop)

		u32 raw;
	};

	static constexpr u32 RAM_DELAY = 6;

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

		FORCE_INLINE u8* GetGuestBase() const {
			return m_guest_base;
		}

		//512 KB, 8 bit bus
		static constexpr u64 BIOS_CONFIG_INIT = 0x013243F;

		//512 KB, 8 bit bus
		static constexpr u64 EXP1_CONFIG_INIT = 0x013243F;

		//128 B, 8 bit bus
		static constexpr u64 EXP2_CONFIG_INIT = 0x0070777;

		//1 Byte
		static constexpr u64 EXP3_CONFIG_INIT = 0x0003022;

		static constexpr u64 RAM_SIZE_INIT = 0xB88;

		static constexpr u64 KUSEG_VOID_START = (u64)512 * 1024 * 1024;
		static constexpr u64 KUSEG_VOID_END = (u64)2 * 1024 * 1024 * 1024;

		/// <summary>
		/// Emulate read (unless fastmem is applicable)
		/// </summary>
		/// <typeparam name="Ty">"Type" that corresponds to the size of the read</typeparam>
		/// <typeparam name="Except">Throw exceptions on invalid reads</typeparam>
		/// <typeparam name="AddCycles">Add access time</typeparam>
		/// <param name="address">Read location</param>
		/// <returns>The bytes at "address"</returns>
		template <typename Ty, bool Except, bool AddCycles = false>
		Ty Read(u32 address) {
			using namespace error;

			if constexpr (sizeof(Ty) != 1) {
				if ((address & (sizeof(Ty) - 1)) != 0) {
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
				DebugBreak();
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
				if constexpr (AddCycles)
					m_curr_cycles += RAM_DELAY;
				return *reinterpret_cast<Ty*>(m_guest_base + address);
			}

			/*
			Test access to expansion regions
			and bios
			*/

			auto compute_access_time = [this](RegionConfig const& reg) {
				auto bus_width_byes = (u32)reg.delay_size.bus_width + 1;
				auto seq_access_count = sizeof(Ty) / bus_width_byes - 1;

				//Add first access
				m_curr_cycles += reg.read_nonseq;

				//Add as many seq. accesses as needed
				m_curr_cycles += reg.read_seq * seq_access_count;
			};

			if (m_exp2_enable && lower >=
				m_exp2_config.base && lower < m_exp2_config.end) {
				if constexpr (AddCycles)
					compute_access_time(m_exp2_config);
				return (Ty)ReadEXP2(lower - m_exp2_config.base);
			}

			if (lower >= m_exp1_config.base && lower < m_exp1_config.end) {
				if constexpr (AddCycles)
					compute_access_time(m_exp1_config);
				return (Ty)ReadEXP1(lower - m_exp1_config.base);
			}

			if (lower >= m_exp3_config.base && lower < m_exp3_config.end) {
				if constexpr (AddCycles)
					compute_access_time(m_exp3_config);
				return (Ty)ReadEXP3(lower - m_exp3_config.base);
			}

			if (lower >= m_bios_config.base && lower < m_bios_config.end) {
				if constexpr (AddCycles)
					compute_access_time(m_bios_config);
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

				if constexpr (AddCycles)
					m_curr_cycles += 1;

				return *reinterpret_cast<Ty*>(m_guest_base + address);
			}

			if (lower >= memory::region_offsets::PSX_IO_OFFSET
				&& lower < memory::region_offsets::PSX_IO_OFFSET +
				memory::region_sizes::PSX_IO_SIZE) {
				if constexpr (AddCycles)
					m_curr_cycles += 1;

				if (!memory::IO::LOCKED[address & (memory::region_sizes::PSX_IO_SIZE - 1)]) {
					return ReadIO<Ty>(address);
				}
				else {
					if constexpr (Except) {
						m_sys_status->exception = true;
						m_sys_status->exception_number =
							cpu::Excode::IBE;
					}
				}

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

		/// <summary>
		/// Emulate write (unless fastmem is applicable)
		/// </summary>
		/// <typeparam name="Ty">"Type" that corresponds to the size of the write</typeparam>
		/// <typeparam name="Except">Throw exceptions on invalid writes</typeparam>
		/// <typeparam name="AddCycles">Add cycles to count</typeparam>
		/// <param name="address">Write location</param>
		/// <param name="value">Value to write</param>
		template <typename Ty, bool Except, bool AddCycles = false>
		void Write(u32 address, Ty value) {
			using namespace error;

			if constexpr (sizeof(Ty) != 1) {
				if ((address & (sizeof(Ty) - 1)) != 0) {
					//Unaligned access!
#ifdef DEBUG
					fmt::print("Unaligned access at 0x{:x}\n", address);
#endif // DEBUG

					if constexpr (Except) {
						m_sys_status->exception = true;
						m_sys_status->exception_number =
							cpu::Excode::ADES;
						m_sys_status->badvaddr = address;
					}

					return;
				}
			}

			bool curr_mode = m_sys_status->curr_mode;

			if (address >= KUSEG_VOID_START && address < KUSEG_VOID_END) {
#ifdef DEBUG
				fmt::print("Writing unused upper 1.5 GB of KUSEG at 0x{:x}\n", address);
#endif // DEBUG
				if constexpr (Except) {
					m_sys_status->exception = true;
					m_sys_status->exception_number =
						cpu::Excode::DBE;
					//Do not set BADVADDR
				}

				return;
			}

			if (address >= memory::KSEG2_START) {
				if (address == memory::IO::CACHE_CONTROL) {
					WriteCacheControl(value);
					return;
				}
			}

			u32 segment = (address >> 29) & 7;
			u32 lower = address & 0x1FFFFFFF;

			if (curr_mode && segment != 0) {
#ifdef DEBUG
				fmt::print("Writing KSEG in USER mode at 0x{:x}\n", address);
#endif // DEBUG
				if constexpr (Except) {
					m_sys_status->exception = true;
					m_sys_status->exception_number =
						cpu::Excode::ADES;
					m_sys_status->badvaddr = address;
				}

				return;
			}

			if (lower < m_ram_end) {
				if constexpr (AddCycles)
					m_curr_cycles += RAM_DELAY;
				*reinterpret_cast<Ty*>(m_guest_base + address) = value;
				return;
			}

			/*
			Test access to expansion regions
			and bios
			*/

			auto compute_access_time = [this](RegionConfig const& reg) {
				auto bus_width_byes = (u32)reg.delay_size.bus_width + 1;
				auto seq_access_count = sizeof(Ty) / bus_width_byes - 1;

				//Add first access
				m_curr_cycles += reg.write_nonseq;

				//Add as many seq. accesses as needed
				m_curr_cycles += reg.write_seq * seq_access_count;
			};

			if (m_exp2_enable && lower >=
				m_exp2_config.base && lower < m_exp2_config.end) {
				WriteEXP2(value, lower - m_exp2_config.base);
				if constexpr (AddCycles)
					compute_access_time(m_exp2_config);
				return;
			}

			if (lower >= m_exp1_config.base && lower < m_exp1_config.end) {
				WriteEXP1(value, lower - m_exp1_config.base);
				if constexpr (AddCycles)
					compute_access_time(m_exp1_config);
				return;
			}

			if (lower >= m_exp3_config.base && lower < m_exp3_config.end) {
				WriteEXP3(value, lower - m_exp3_config.base);
				if constexpr (AddCycles)
					compute_access_time(m_exp3_config);
				return;
			}

			if (lower >= m_bios_config.base && lower < m_bios_config.end) {
				if constexpr (AddCycles)
					compute_access_time(m_bios_config);
				return;
			}

			if (lower >= memory::region_offsets::PSX_SCRATCHPAD_OFFSET
				&& lower < memory::region_offsets::PSX_SCRATCHPAD_OFFSET +
				memory::region_sizes::PSX_SCRATCHPAD_PADDED_SIZE) {
				if (segment == 0x5) {
#ifdef DEBUG
					fmt::print("Writing scratchpad in KSEG1 at 0x{:x}\n", address);
#endif // DEBUG
					if constexpr (Except) {
						m_sys_status->exception = true;
						m_sys_status->exception_number =
							cpu::Excode::DBE;
					}

					return;
				}

				if constexpr (AddCycles)
					m_curr_cycles += 1;

				*reinterpret_cast<Ty*>(m_guest_base + address) = value;
				return;
			}

			if (lower >= memory::region_offsets::PSX_IO_OFFSET
				&& lower < memory::region_offsets::PSX_IO_OFFSET +
				memory::region_sizes::PSX_IO_SIZE) {
				if (!memory::IO::LOCKED[address & (memory::region_sizes::PSX_IO_SIZE - 1)]) {
					WriteIO<Ty>(address, value);
				}
				else {
					if constexpr (Except) {
						m_sys_status->exception = true;
						m_sys_status->exception_number =
							cpu::Excode::DBE;
					}
				}
				
				if constexpr (AddCycles)
					m_curr_cycles += 1;
				return;
			}

			if constexpr (Except) {
#ifdef DEBUG
				fmt::print("Writing unused memory at 0x{:x}\n", address);
#endif // DEBUG
				m_sys_status->exception = true;
				m_sys_status->exception_number =
					cpu::Excode::DBE;
			}
		}

		/// <summary>
		/// Write to the IO region
		/// </summary>
		/// <typeparam name="Ty">Type with size of write</typeparam>
		/// <param name="address">Full address (not offset in IO)</param>
		/// <param name="value">Value to write</param>
		template <typename Ty>
		void WriteIO(u32 address, Ty value) {
			address &= 0xFFF;

			if (address >= memory::IO::MEM_CONTROL_START && address < memory::IO::MEM_CONTROL_END) {
				u32 to_write = value;
				
				if constexpr (sizeof(Ty) != 4) {
					u32 shift = (address & 3) * 8;
					to_write <<= shift;
				}

				WriteMemControl(address & ~3, to_write);
				return;
			}

			if (address >= memory::IO::RAM_SIZE &&
				address < memory::IO::RAM_SIZE + 4) {
				u32 to_write = value;

				if constexpr (sizeof(Ty) != 4) {
					u32 shift = (address & 3) * 8;
					to_write <<= shift;
				}

				ReconfigureRAM(to_write);
				return;
			}

			if (address >= memory::IO::INTERRUPT_STAT &&
				address < memory::IO::INTERRUPT_STAT + 4) {
				u32 to_write = value;

				if constexpr (sizeof(Ty) != 4) {
					u32 shift = (address & 3) * 8;
					to_write <<= shift;
				}

				for (u32 bit_index = 0; bit_index < 10; bit_index++) {
					bool ack_bit = !!((to_write >> bit_index) & 1);
					//bool req_bit = !!((m_sys_status->interrupt_request >> bit_index) & 1);

					if (!ack_bit)
						m_sys_status->interrupt_request &= ~((u32)1 << bit_index);
				}
				
				return;
			}

			if (address >= memory::IO::INTERRUPT_MASK &&
				address < memory::IO::INTERRUPT_MASK + 4) {
				u32 to_write = value;

				if constexpr (sizeof(Ty) != 4) {
					u32 shift = (address & 3) * 8;
					to_write <<= shift;
				}

				m_sys_status->interrupt_mask = to_write;
				return;
			}

			if ((address & 0xFF0) == memory::IO::TIMER_1) {
				u32 to_write = value;

				if constexpr (sizeof(Ty) != 4) {
					u32 shift = (address & 3) * 8;
					to_write <<= shift;
				}

				m_count1.Write(address - memory::IO::TIMER_1, to_write);

				return;
			}

			if ((address & 0xFF0) == memory::IO::TIMER_2) {
				u32 to_write = value;

				if constexpr (sizeof(Ty) != 4) {
					u32 shift = (address & 3) * 8;
					to_write <<= shift;
				}

				m_count2.Write(address - memory::IO::TIMER_2, to_write);

				return;
			}

			if ((address & 0xFF0) == memory::IO::TIMER_3) {
				u32 to_write = value;

				if constexpr (sizeof(Ty) != 4) {
					u32 shift = (address & 3) * 8;
					to_write <<= shift;
				}

				m_count3.Write(address - memory::IO::TIMER_3, to_write);

				return;
			}

			if (address >= memory::IO::DMA_START &&
				address < memory::IO::DMA_END) {
				u32 to_write = value;

				if constexpr (sizeof(Ty) != 4) {
					u32 shift = (address & 3) * 8;
					to_write <<= shift;
				}

				u32 mask = 0xFF'FF'FF'FF;

				if constexpr (sizeof(Ty) == 1)
					mask = 0xFF;
				else if constexpr (sizeof(Ty) == 2)
					mask = 0xFFFF;

				m_dma_controller.Write(address,
					to_write, mask << ((address & 3) * 8));
				return;
			}

			if (address >= memory::IO::SPU_START &&
				address < memory::IO::SPU_END)
				return;

			if (address >= GP0_ADD && address < GP0_ADD + 4) {
				u32 to_write = value;

				if constexpr (sizeof(Ty) != 4) {
					u32 shift = (address & 3) * 8;
					to_write <<= shift;
				}

				m_gpu.WriteGP0(to_write);

				return;
			}

			if (address >= GP1_ADD && address < GP1_ADD + 4) {
				u32 to_write = value;

				if constexpr (sizeof(Ty) != 4) {
					u32 shift = (address & 3) * 8;
					to_write <<= shift;
				}

				m_gpu.WriteGP1(to_write);

				return;
			}

#ifdef DEBUG_IO
			fmt::println("Write to invalid/unused/unimplemented register 0x{:x}", address);
#endif // DEBUG_IO

		}

		template <typename Ty>
		Ty ReadIO(u32 address) {
			address &= 0xFFF;

			if (address >= memory::IO::MEM_CONTROL_START && address < memory::IO::MEM_CONTROL_END) {
				u32 shift = (address & 3) * 8;

				return (Ty)(ReadMemControl(address & ~3) >> shift);
			}

			if (address >= memory::IO::RAM_SIZE &&
				address < memory::IO::RAM_SIZE + 4) {
				u32 shift = (address & 3) * 8;

				return (Ty)(m_ram_config >> shift);
			}

			if (address >= memory::IO::INTERRUPT_MASK &&
				address < memory::IO::INTERRUPT_MASK + 4) {
				u32 shift = (address & 3) * 8;

				return (Ty)(m_sys_status->interrupt_mask >> shift);
			}

			if (address >= memory::IO::INTERRUPT_STAT &&
				address < memory::IO::INTERRUPT_STAT + 4) {
				u32 shift = (address & 3) * 8;

				return (Ty)(m_sys_status->interrupt_request >> shift);
			}

			if ((address & 0xFF0) == memory::IO::TIMER_1) {
				u32 shift = (address & 3) * 8;

				return (Ty)(m_count1.Read(address - memory::IO::TIMER_1) >> shift);
			}

			if ((address & 0xFF0) == memory::IO::TIMER_2) {
				u32 shift = (address & 3) * 8;

				return (Ty)(m_count2.Read(address - memory::IO::TIMER_2) >> shift);
			}

			if ((address & 0xFF0) == memory::IO::TIMER_3) {
				u32 shift = (address & 3) * 8;

				return (Ty)(m_count3.Read(address - memory::IO::TIMER_3) >> shift);
			}

			if (address >= memory::IO::DMA_START &&
				address < memory::IO::DMA_END) {
				u32 shift = (address & 3) * 8;

				return (Ty)(m_dma_controller.Read(address) >> shift);
			}

			if (address >= memory::IO::SPU_START &&
				address < memory::IO::SPU_END)
				return 0;

			if (address >= GP0_ADD && address < GP0_ADD + 4) {
				u32 shift = (address & 3) * 8;

				return (Ty)(m_gpu.ReadData() >> shift);
			}

			if (address >= GP1_ADD && address < GP1_ADD + 4) {
				u32 shift = (address & 3) * 8;

				return (Ty)(m_gpu.ReadStat() >> shift);
			}

#ifdef DEBUG_IO
			fmt::println("Reading invalid/unused/unimplemented register 0x{:x}", address);
#endif // DEBUG_IO

			return 0x0;
		}

		void LoadBios(u8* data, u32 size);

		/// <summary>
		/// Cycles in the accumulated during
		/// the current instruction
		/// </summary>
		u64 m_curr_cycles;

		FORCE_INLINE bool CacheEnabled() const {
			return m_cache_control.cache_en;
		}

		RootCounter& GetCounter0() {
			return m_count1;
		}

		RootCounter& GetCounter1() {
			return m_count2;
		}

		RootCounter& GetCounter2() {
			return m_count3;
		}

		Gpu& GetGPU() {
			return m_gpu;
		}

		DmaController& GetDMAControl() {
			return m_dma_controller;
		}

		friend class DebugView;

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

		bool SetBiosMap(u32 new_size, bool read_only);
		bool ResetBiosMap();

		void ComputeDelays(RegionConfig& conf);

		void ReconfigureBIOS(u32 new_config);
		void ReconfigureRAM(u32 ram_conf);

		void WriteCOMDelay(u32 value);
		void WriteConf(RegionConfig& conf, u32 value);

		void WriteMemControl(u32 address, u32 value);

		void WriteCacheControl(u32 value);

		void WriteEXP1(u32 value, u32 address);
		void WriteEXP2(u32 value, u32 address);
		void WriteEXP3(u32 value, u32 address);

		u32 ReadEXP1(u32 address);
		u32 ReadEXP2(u32 address);
		u32 ReadEXP3(u32 address);

		u32 ReadMemControl(u32 address) const;
		u32 ReadCacheControl() const;

	private :
		system_status* m_sys_status;

		memory::MemoryMapper* m_mapper;
		u8* m_guest_base;

		RamSize m_curr_ram_sz;
		u32 m_ram_end;
		u32 m_ram_config;
		CacheControl m_cache_control;
		RegionConfig m_bios_config;
		RegionConfig m_exp1_config;
		RegionConfig m_exp2_config;
		RegionConfig m_exp3_config;
		RegionConfig m_spu_config;
		RegionConfig m_cdrom_config;

		bool m_exp2_enable;

		ComDelay m_com_delays;

		RootCounter m_count1;
		RootCounter m_count2;
		RootCounter m_count3;

		DmaController m_dma_controller;

		Gpu m_gpu;
	};
}