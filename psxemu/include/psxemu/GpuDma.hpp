#pragma once

#include <psxemu/include/psxemu/DmaBase.hpp>

namespace psx {
	class GpuDma : public DmaBase {
	public :
		GpuDma(system_status* sys_status, DmaController* dma_controller);

		void AdvanceTransfer() override;

		void DreqRisingEdge();

		void SetDreq(bool dreq) {
			m_dreq = dreq;
		}

	private :
		bool m_dreq;
		bool m_keep_going;
		bool m_running;
	};
}