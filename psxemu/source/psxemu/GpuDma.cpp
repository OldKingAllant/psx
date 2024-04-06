#include <psxemu/include/psxemu/GpuDma.hpp>

namespace psx {
	GpuDma::GpuDma(system_status* sys_status, DmaController* dma_controller) :
		DmaBase(sys_status, dma_controller, 2), 
		m_dreq{}, m_keep_going{}
	{}

	void GpuDma::DreqRisingEdge() {
		if (!m_control.start_busy || m_running)
			return;

		if (!m_transfer_active)
			TransferStart(false);
		else
			TransferStart(true);
	}
}