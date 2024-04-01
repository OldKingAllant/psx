#pragma once

#include <common/Defs.hpp>

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
		struct {
			u8 interrupt_on_block : 7;
			u8 : 8;
			bool bus_error : 1;
			u8 channel_int_enable : 7;
			bool master_int_enable : 1;
			u8 channel_int_req : 7;
			bool master_irq : 1;
		};

		u32 raw;
	};

	class DmaController {
	public :
		DmaController(system_status* sys_status);

		void Write(u32 address, u32 value, u32 mask);
		u32 Read(u32 address) const;

	private :
		system_status* m_sys_status;

		DPCR m_control;
		DICR m_int_control;
	};
}