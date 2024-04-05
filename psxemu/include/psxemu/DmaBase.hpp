#pragma once

#include <psxemu/include/psxemu/DmaCommon.hpp>

namespace psx {
	struct system_status;
	class DmaController;

	static constexpr u32 LINKED_MAX_NODE_COUNT = 4096;

	class DmaBase {
	public:
		DmaBase(system_status* sys_status, DmaController* dma_controller, u32 id);

		void Write(u32 address, u32 value, u32 mask);
		u32 Read(u32 address);

		virtual void AdvanceTransfer();

		virtual void DoLinked();
		virtual void DoBurst();

		void TransferStart(bool resume);
		void TransferEnd(bool last_block);

		bool Dreq() const {
			return true;
		}

		virtual u32 GetPort() const = 0;

	protected:
		system_status* m_sys_status;
		DmaController* m_controller;

		u32 m_shadow_base_address;
		u32 m_shadow_block_control;
		u32 m_base_address;
		u32 m_block_control;
		CHCR m_control;
		u32 m_words_rem;
		u32 m_blocks_rem;
		u32 m_next_block_add;
		u32 m_curr_address;

		u32 m_id;
		bool m_running;
		bool m_transfer_active;

		u32 m_linked_list_node_count;
	};
}