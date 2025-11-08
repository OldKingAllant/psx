#pragma once

#include <common/Defs.hpp>

#include <psxemu/include/psxemu/OTDma.hpp>
#include <psxemu/include/psxemu/GpuDma.hpp>
#include <psxemu/include/psxemu/MDECInDma.hpp>
#include <psxemu/include/psxemu/MDECOutDma.hpp>
#include <psxemu/include/psxemu/CDROMDma.hpp>

class DebugView;

namespace psx {
	struct system_status;

	static constexpr u64 DMA0_ADD = 0x80;
	static constexpr u64 DMA1_ADD = 0x90;
	static constexpr u64 DMA2_ADD = 0xA0;
	static constexpr u64 DMA3_ADD = 0xB0;
	static constexpr u64 DMA4_ADD = 0xC0;
	static constexpr u64 DMA5_ADD = 0xD0;
	static constexpr u64 DMA6_ADD = 0xE0;

	static constexpr u64 DMA_CONTROL = 0xF0;
	static constexpr u64 DMA_INT = 0xF4;

	union DPCR {
#pragma pack(push, 1)
		struct {
			u8 dma0_prio : 3;
			bool dma0_en : 1;

			u8 dma1_prio : 3;
			bool dma1_en : 1;

			u8 dma2_prio : 3;
			bool dma2_en : 1;

			u8 dma3_prio : 3;
			bool dma3_en : 1;

			u8 dma4_prio : 3;
			bool dma4_en : 1;

			u8 dma5_prio : 3;
			bool dma5_en : 1;

			u8 dma6_prio : 3;
			bool dma6_en : 1;

			u8 cpu_access_prio : 3;
		};
#pragma pack(pop)

		u32 raw;
	};

	union DICR {
#pragma pack(push, 1)
		struct {
			u8 interrupt_on_block : 7;
			u8 : 1;
			u8 : 0;
			u8 : 7;
			bool bus_error : 1;
			u8 channel_int_enable : 7;
			bool master_int_enable : 1;
			u8 channel_int_req : 7;
			bool master_irq : 1;
		};
#pragma pack(pop)

		u32 raw;
	};

	struct Transfer {
		u8 dma_id;
		u8 dma_priority;
	};

	class DmaController {
	public :
		DmaController(system_status* sys_status);

		void Write(u32 address, u32 value, u32 mask);
		u32 Read(u32 address);

		OTDma& GetOtDma() {
			return m_ot_dma;
		}

		GpuDma& GetGpuDma() {
			return m_gpu_dma;
		}

		MDECIn& GetMDECInDma() {
			return m_mdecin_dma;
		}

		MDECOut& GetMDECOutDma() {
			return m_mdecout_dma;
		}

		CDROMDma& GetCDROMDma() {
			return m_cdrom_dma;
		}

		void AddTransfer(u8 dma_id);
		void RemoveTransfer();

		bool HasActiveTransfer() const {
			return m_num_active;
		}

		void AdvanceTransfer();

		void SignalException();
		void InterruptRequest(u8 dma_id, bool last_block);

		void UpdateMasterIRQ();

		bool ChannelEnabled(u8 dma_id) const {
			return (bool)((m_control.raw >> (dma_id * 4 + 3)) & 1);
		}

		inline void EnableFastDma(bool enable) {
			m_enable_fast_dma = enable;
		}

		inline void UseSimd(bool enable) {
			m_use_simd = enable;
		}

		inline bool IsFastDmaEnabled() const {
			return m_enable_fast_dma;
		}

		inline bool IsSimdEnabled() const {
			return m_use_simd;
		}

		friend class DebugView;

	private :
		system_status* m_sys_status;

		DPCR m_control;
		DICR m_int_control;

		OTDma   m_ot_dma;
		GpuDma  m_gpu_dma;
		MDECIn  m_mdecin_dma;
		MDECOut m_mdecout_dma;
		CDROMDma m_cdrom_dma;

		Transfer m_active_dmas[8];
		u8 m_num_active;

		bool m_enable_fast_dma;
		bool m_use_simd;
	};
}