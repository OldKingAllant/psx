#include <psxemu/include/psxemu/CDROMDma.hpp>
#include <psxemu/include/psxemu/SystemStatus.hpp>
#include <psxemu/include/psxemu/SystemBus.hpp>

#include <psxemu/include/psxemu/Logger.hpp>
#include <psxemu/include/psxemu/LoggerMacros.hpp>

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

	void CDROMDma::DoBurst() {
		if (m_words_rem == 0) {
			TransferEnd(true);
			return;
		}

		auto sysbus = m_sys_status->sysbus;
		u32 port = GetPort();

		if (!m_control.transfer_dir) {
			u32 data_low = sysbus->Read<u16, true, false>(port);
			u32 data_high = sysbus->Read<u16, true, false>(port);
			u32 data = (data_high << 16) | data_low;
			sysbus->Write<u32, true, false>(m_curr_address, data);
		}
		else {
			LOG_ERROR("DMA", "[DMA] RAM -> CDROM transfers are not permitted");
			error::DebugBreak();
		}

		if (m_sys_status->exception)
			m_controller->SignalException();

		if (m_control.decrement)
			m_curr_address -= 4;
		else
			m_curr_address += 4;

		m_curr_address &= 0xFFFFFC;

		m_words_rem--;
		sysbus->m_curr_cycles += 1;
	}

}