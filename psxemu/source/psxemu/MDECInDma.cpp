#include <psxemu/include/psxemu/MDECInDma.hpp>

namespace psx {
	MDECIn::MDECIn(system_status* sys_status, DmaController* dma_controller) :
		DmaBase(sys_status, dma_controller, 0),
		m_dreq{false}
	{}

	void MDECIn::DreqRisingEdge() {
		if (!m_control.start_busy || m_running)
			return;

		if (!m_transfer_active)
			TransferStart(false);
		else
			TransferStart(true);
	}
}


