#pragma once

#include <psxemu/include/psxemu/OTDma.hpp>

#include <psxemu/include/psxemu/SystemStatus.hpp>
#include <psxemu/include/psxemu/SystemBus.hpp>
#include <psxemu/include/psxemu/DmaController.hpp>

namespace psx {
	OTDma::OTDma(system_status* sys_status, DmaController* dma_controller) :
		DmaBase(sys_status, dma_controller, 6)
	{
		m_control.decrement = true;
	}

	void OTDma::Write(u32 address, u32 value, u32 mask) {
		if (address >= MADR && address < MADR + 4) {
			m_base_address = value;
			m_base_address &= 0xFFFFFF;
			return;
		}

		if (address >= BLOCK_CONTROL && address < BLOCK_CONTROL + 4) {
			m_block_control &= ~mask;
			m_block_control |= (value & mask);
			return;
		}

		if (address >= CHCR_ADD && address < CHCR_ADD + 4) {
			value &= CONTROL_WRITE_MASK;

			bool old_start = m_control.start_busy;
			bool old_force = m_control.force_start;

			m_control.raw = value;
			m_control.decrement = true;

			bool start_edge = !old_start && m_control.start_busy;
			bool force_edge = !old_force && m_control.force_start;

			if (start_edge || force_edge)
				TransferStart(false);
			return;
		}

		fmt::println("[OT DMA] Accessing invalid/unused register 0x{:x}", address);
	}

	void OTDma::AdvanceTransfer() {
		auto sysbus = m_sys_status->sysbus;

		auto block_control = std::bit_cast<BurstBlockControl*>(&m_shadow_block_control);
		block_control->word_count--;

		u32 address = (block_control->word_count == 0) ?
			0x00FFFFFF : (m_shadow_base_address - 0x4);

		sysbus->Write<u32, true, false>(m_shadow_base_address, address & 0xFFFFFF);

		if (m_sys_status->exception)
			m_controller->SignalException();

		m_shadow_base_address -= 4;

		sysbus->m_curr_cycles += 1;

		if (block_control->word_count == 0)
			TransferEnd(true);
	}
}