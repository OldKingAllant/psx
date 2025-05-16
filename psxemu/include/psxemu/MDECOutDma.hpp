#pragma once

#include <psxemu/include/psxemu/DmaBase.hpp>

namespace psx {
	class MDECOut : public DmaBase {
	public:
		MDECOut(system_status* sys_status, DmaController* dma_controller);

		static constexpr u32 MDEC0 = 0x1F801820;

		u32 GetPort() const override {
			return MDEC0;
		}

		void DreqRisingEdge();

		void SetDreq(bool dreq) {
			m_dreq = dreq;
		}

		bool Dreq() const {
			return m_dreq;
		}

	private:
		bool m_dreq;
	};
}