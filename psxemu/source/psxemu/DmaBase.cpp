#include <psxemu/include/psxemu/DmaBase.hpp>
#include <psxemu/include/psxemu/DmaController.hpp>
#include <psxemu/include/psxemu/SystemStatus.hpp>
#include <psxemu/include/psxemu/SystemBus.hpp>

#include <fmt/format.h>

#include <common/Errors.hpp>

namespace psx {
	DmaBase::DmaBase(system_status* sys_status, DmaController* dma_controller, u32 id) :
		m_sys_status{ sys_status }, m_controller{ dma_controller },
		m_shadow_base_address{}, m_shadow_block_control{},
		m_base_address{}, m_block_control{},
		m_control{}, m_words_rem{}, m_blocks_rem{},
		m_next_block_add{}, m_id{id}, m_running{}, m_curr_address{}, 
		m_transfer_active{}, m_linked_list_node_count{}
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

			bool start_edge = !old_start && m_control.start_busy;
			bool force_edge = !old_force && m_control.force_start;

			if (force_edge || (start_edge && m_control.sync == SyncMode::BURST)
				|| (start_edge && Dreq()))
				TransferStart(false);
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

	void DmaBase::TransferStart(bool resume) {
		if (!m_controller->ChannelEnabled(m_id))
			return;

		if (m_control.chopping) {
			fmt::println("[DMA{}] Chopping mode not supported!", m_id);
			error::DebugBreak();
		}

		m_transfer_active = true;

		m_shadow_base_address = m_base_address;
		m_shadow_block_control = m_block_control;
		m_curr_address = m_shadow_base_address;

		if ((m_shadow_block_control & 0xFFFF) == 0 && 
			m_control.sync == SyncMode::BURST)
			m_shadow_block_control = 0x10000;

		if(m_control.pause)
			fmt::println("[DMA{}] Pause bit set", m_id);

		switch (m_control.sync)
		{
		case SyncMode::BURST:
			m_words_rem = m_shadow_block_control & 0xFFFF;
			break;
		case SyncMode::SLICE: {
			auto block_control = *reinterpret_cast<SliceBlockControl*>(&m_shadow_block_control);
			m_words_rem = block_control.blocksize;
			
			if (!resume)
				m_blocks_rem = block_control.block_count;
		}
			break;
		case SyncMode::LINKED: {
			if(m_control.decrement)
				fmt::println("[DMA{}] Decrementing MADR in linked mode", m_id);
			else if(!m_control.transfer_dir && m_control.sync == SyncMode::LINKED)
				fmt::println("[DMA{}] Linked mode DEV to RAM", m_id);

			if (!resume) {
				u32 header = m_sys_status->sysbus->Read<u32, false, false>(m_curr_address);
				m_words_rem = (header >> 24) & 0xFF;
				m_next_block_add = (header & 0xFFFFFC);
				m_curr_address += 4;
			}
			else {
				m_curr_address = m_next_block_add + 4;
				u32 header = m_sys_status->sysbus->Read<u32, false, false>(m_next_block_add);
				m_words_rem = (header >> 24) & 0xFF;
				m_next_block_add = (header & 0xFFFFFC);
			}
		}
			break;
		default:
			fmt::println("[DMA{}] Using reserved sync mode!", m_id);
			return;
			break;
		}

		//Cleared on transfer start
		m_control.force_start = false;

		m_controller->AddTransfer(m_id);

		m_running = true;
	}

	void DmaBase::TransferEnd(bool last_block) {
		if (last_block) {
			m_control.start_busy = false;
			m_transfer_active = false;
		}

		m_controller->RemoveTransfer();
		m_controller->InterruptRequest(m_id, last_block);

		m_running = false;
	}

	void DmaBase::DoLinked() {
		constexpr u32 END_MARKER = 0x800000;

		if (m_words_rem == 0) {
			if (m_next_block_add & END_MARKER) {
				TransferEnd(true);
			}
			else if (Dreq()) {
				m_controller->InterruptRequest((u8)m_id, false);
				m_curr_address = m_next_block_add + 4;
				u32 header = m_sys_status->sysbus->Read<u32, false, false>(m_next_block_add);
				m_words_rem = (header >> 24) & 0xFF;
				m_next_block_add = (header & 0xFFFFFC);
			}
			else {
				TransferEnd(false);
			}

			return;
		}

		auto sysbus = m_sys_status->sysbus;

		u32 port = GetPort();

		if (!m_control.transfer_dir) {
			error::DebugBreak();
		}
		else {
			//RAM to device
			u32 data = sysbus->Read<u32, true, false>(m_curr_address);
			sysbus->Write<u32, true, false>(port, data);
		}

		if (m_sys_status->exception)
			m_controller->SignalException();

		if (m_control.decrement)
			m_curr_address -= 4;
		else
			m_curr_address += 4;

		m_words_rem--;
	}

	void DmaBase::DoBurst() {
		//
	}

	void DmaBase::AdvanceTransfer() {
		switch (m_control.sync)
		{
		case SyncMode::LINKED:
			DoLinked();
			break;
		default:
			error::DebugBreak();
			break;
		}
	}
}