#include <psxemu/include/psxemu/SPUDma.hpp>

#include <psxemu/include/psxemu/Logger.hpp>
#include <psxemu/include/psxemu/LoggerMacros.hpp>
#include <psxemu/include/psxemu/SystemStatus.hpp>
#include <psxemu/include/psxemu/SystemBus.hpp>

#include <psxemu/include/psxemu/SPU.hpp>

namespace psx {
	SPUDma::SPUDma(system_status* sys_status, DmaController* dma_controller) :
		DmaBase(sys_status, dma_controller, 4),
		m_dreq{ false }
	{}

#pragma optimize("", off)
	void SPUDma::DreqRisingEdge() {
		if (!m_control.start_busy || m_running)
			return;

		if (!m_transfer_active)
			TransferStart(false);
		else
			TransferStart(true);
	}

	void SPUDma::AdvanceTransfer() {
		if (m_control.sync != SyncMode::SLICE) {
			LOG_ERROR("DMA", "[DMA4] SPU DMA can only use block transfer");
			TransferEnd(true);
			return;
		}

		auto block_control_ptr = reinterpret_cast<SliceBlockControl*>(&m_shadow_block_control);
		if (block_control_ptr->blocksize * 4 != SPU::FIFO_SIZE_BYTES) {
			LOG_ERROR("DMA", "[DMA4] SPU DMA transfer with blocksize != 0x10");
			TransferEnd(true);
			return;
		}

		if (m_words_rem == 0) {
			m_blocks_rem--;

			auto block_control_ptr = reinterpret_cast<SliceBlockControl*>(&m_shadow_block_control);
			block_control_ptr->block_count -= 1;

			if (m_blocks_rem == 0 || (int)m_blocks_rem == -1) {
				TransferEnd(true);
				m_dreq = false;
			}
			else {
				m_controller->InterruptRequest((u8)m_id, false);
				auto block_control = *reinterpret_cast<SliceBlockControl*>(&m_shadow_block_control);
				m_words_rem = block_control.blocksize;
			}

			return;
		}

		auto sysbus = m_sys_status->sysbus;
		auto& spu = m_sys_status->sysbus->GetSPU();

		if (!m_control.transfer_dir) {
			u32 data = spu.DmaRead();
			sysbus->Write<u32, true, false>(m_curr_address, data);
		}
		else {
			//RAM to device
			u32 data = sysbus->Read<u32, true, false>(m_curr_address);
			spu.DmaWrite(data);
		}

		if (m_sys_status->exception)
			m_controller->SignalException();

		if (m_control.decrement)
			m_curr_address -= 4;
		else
			m_curr_address += 4;

		m_curr_address &= 0xFFFFFC;

		m_words_rem--;
		sysbus->m_curr_cycles += 2;
	}
#pragma optimize("", on)
}