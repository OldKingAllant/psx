#include <psxemu/include/psxemu/CDROMDma.hpp>

namespace psx {
	CDROMDma::CDROMDma(system_status* sys_status, DmaController* dma_controller) :
		DmaBase(sys_status, dma_controller, 3),
		m_dreq{ false }
	{}

	void CDROMDma::DreqRisingEdge() {
		if (!m_control.start_busy || m_running)
			return;

		if (!m_transfer_active)
			TransferStart(false);
		else
			TransferStart(true);
	}
}