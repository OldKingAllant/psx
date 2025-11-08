#pragma once

#include <psxemu/include/psxemu/DmaCommon.hpp>
#include <psxemu/include/psxemu/DmaBase.hpp>

namespace psx {
	struct system_status;
	class DmaController;

	class OTDma : public DmaBase {
	public :
		OTDma(system_status* sys_status, DmaController* dma_controller);

		static constexpr u32 CONTROL_WRITE_MASK = 0b01010001000000000000000000000000;

		void Write(u32 address, u32 value, u32 mask);

		void AdvanceTransfer() override;

		void FastTransfer();
		void SimdTransfer();

		u32 GetPort() const override { return 0; }
	};
}
