#include <psxemu/include/psxemu/MDECOutDma.hpp>

namespace psx {
	MDECOut::MDECOut(system_status* sys_status, DmaController* dma_controller) :
		DmaBase(sys_status, dma_controller, 1),
		m_dreq{false}
	{}

	void MDECOut::DreqRisingEdge() {
		if (!m_control.start_busy || m_running)
			return;

		if (!m_transfer_active)
			TransferStart(false);
		else
			TransferStart(true);
	}
}