#pragma once

#include <psxemu/include/psxemu/DmaBase.hpp>

namespace psx {
	class GpuDma : public DmaBase {
	public :
		GpuDma(system_status* sys_status, DmaController* dma_controller);

		void DreqRisingEdge();

		void SetDreq(bool dreq) {
			m_dreq = dreq;
		}

		bool Dreq() const {
			return m_dreq;
		}

		static constexpr u32 GP0 = 0x1F801810;

		u32 GetPort() const override { 
			return GP0; 
		}

	private :
		bool m_dreq;
		bool m_keep_going;
	};
}