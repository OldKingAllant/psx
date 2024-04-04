#pragma once

#include <psxemu/include/psxemu/DmaCommon.hpp>

namespace psx {
	struct system_status;
	class DmaController;

	class DmaBase {
	public:
		DmaBase(system_status* sys_status, DmaController* dma_controller, u32 id);

		void Write(u32 address, u32 value, u32 mask);
		u32 Read(u32 address);

		virtual void AdvanceTransfer() = 0;

		void TransferStart();
		void TransferEnd();

	protected:
		system_status* m_sys_status;
		DmaController* m_controller;

		u32 m_shadow_base_address;
		u32 m_shadow_block_control;
		u32 m_base_address;
		u32 m_block_control;
		CHCR m_control;
		u32 m_curr_word_count;

		u32 m_id;
	};
}