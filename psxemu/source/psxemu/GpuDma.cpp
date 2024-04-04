#include <psxemu/include/psxemu/GpuDma.hpp>

namespace psx {
	GpuDma::GpuDma(system_status* sys_status, DmaController* dma_controller) :
		DmaBase(sys_status, dma_controller, 2), 
		m_dreq{}, m_keep_going{}, 
		m_running{}
	{}

	void GpuDma::AdvanceTransfer() {

	}

	void GpuDma::DreqRisingEdge() {

	}
}