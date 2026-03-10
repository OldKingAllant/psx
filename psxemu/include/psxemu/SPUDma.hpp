#pragma once

#include <psxemu/include/psxemu/DmaBase.hpp>

namespace psx {
	class SPUDma : public DmaBase {
	public:
		SPUDma(system_status* sys_status, DmaController* dma_controller);

		static constexpr u32 SPU_FIFO = 0x1F801DA8;

		u32 GetPort() const override {
			return SPU_FIFO;
		}

		void DreqRisingEdge();

		void SetDreq(bool dreq) {
			m_dreq = dreq;
		}

		bool Dreq() const override {
			return m_dreq;
		}

		void AdvanceTransfer() override;

	private:
		bool m_dreq;
	};
}