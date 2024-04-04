#include <psxemu/include/psxemu/DmaBase.hpp>
#include <psxemu/include/psxemu/DmaController.hpp>

#include <fmt/format.h>

namespace psx {
	DmaBase::DmaBase(system_status* sys_status, DmaController* dma_controller, u32 id) :
		m_sys_status{ sys_status }, m_controller{ dma_controller },
		m_shadow_base_address{}, m_shadow_block_control{},
		m_base_address{}, m_block_control{},
		m_control{}, m_curr_word_count{}, 
		m_id{id}
	{}

	void DmaBase::Write(u32 address, u32 value, u32 mask) {
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
			bool old_start = m_control.start_busy;
			bool old_force = m_control.force_start;

			m_control.raw = value;
			m_control.decrement = true;

			bool start_edge = !old_start && m_control.start_busy;
			bool force_edge = !old_force && m_control.force_start;

			if (force_edge)
				TransferStart();
			return;
		}

		fmt::println("[DMA{}] Accessing invalid/unused register 0x{:x}", address, m_id);
	}

	u32 DmaBase::Read(u32 address) {
		if (address >= MADR && address < MADR + 4) {
			return m_base_address;
		}

		if (address >= BLOCK_CONTROL && address < BLOCK_CONTROL + 4) {
			return m_block_control;
		}

		if (address >= CHCR_ADD && address < CHCR_ADD + 4) {
			return m_control.raw;
		}

		fmt::println("[DMA{}] Accessing invalid/unused register 0x{:x}", address, m_id);
		return 0;
	}

	void DmaBase::TransferStart() {
		m_shadow_base_address = m_base_address;
		m_shadow_block_control = m_block_control;

		if (m_shadow_base_address == 0 && 
			m_control.sync == SyncMode::BURST)
			m_shadow_base_address = 0x10000;

		//Cleared on transfer start
		m_control.force_start = false;

		m_controller->AddTransfer(m_id);
	}

	void DmaBase::TransferEnd() {
		m_control.start_busy = false;

		m_controller->RemoveTransfer();
		m_controller->InterruptRequest(m_id);
	}
}