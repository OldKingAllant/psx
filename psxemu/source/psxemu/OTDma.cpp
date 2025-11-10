#pragma once

#include <psxemu/include/psxemu/OTDma.hpp>

#include <psxemu/include/psxemu/SystemStatus.hpp>
#include <psxemu/include/psxemu/SystemBus.hpp>
#include <psxemu/include/psxemu/DmaController.hpp>

#include <immintrin.h>
#include <emmintrin.h>

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
		if (m_controller->IsFastDmaEnabled()) {
			FastTransfer();
		}
		else {
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
		}
		
		auto block_control = std::bit_cast<BurstBlockControl*>(&m_shadow_block_control);
		if (block_control->word_count == 0)
			TransferEnd(true);
	}

	void OTDma::FastTransfer() {
		if (m_controller->IsSimdEnabled()) {
			SimdTransfer();
		}

		u32* ram_base = std::bit_cast<u32*>( m_sys_status->sysbus->GetRamBase() );

		auto block_control = std::bit_cast<BurstBlockControl*>(&m_shadow_block_control);
		m_sys_status->sysbus->m_curr_cycles += block_control->word_count * 1ULL;
		while (block_control->word_count > 0) {
			block_control->word_count--;
			u32 address = (block_control->word_count == 0) ?
				0x00FFFFFF : (m_shadow_base_address - 0x4);

			ram_base[(m_shadow_base_address & (memory::region_sizes::PSX_MAIN_RAM_SIZE - 1)) >> 2] =
				address;

			m_shadow_base_address -= 4;
		}
	}

	void OTDma::SimdTransfer() {
		u32* ram_base = std::bit_cast<u32*>(m_sys_status->sysbus->GetRamBase());

		auto block_control = std::bit_cast<BurstBlockControl*>(&m_shadow_block_control);
		auto block_count = block_control->word_count >> 3;

		m_sys_status->sysbus->m_curr_cycles += (u64(block_count) << 3) * 1ULL;

		while (block_count) {
			block_count--;
			__m256i block = _mm256_set1_epi32(m_shadow_base_address);
			block = _mm256_sub_epi32(block, _mm256_set_epi32(4, 8, 12, 16, 20, 24, 28, 32));
			block = _mm256_insert_epi32(block, block_count == 0 ? 0x00FFFFFF :
				_mm256_cvtsi256_si32(block), 0);
			m_shadow_base_address -= 32;
			u64 write_address = (m_shadow_base_address & (memory::region_sizes::PSX_MAIN_RAM_SIZE - 1)) >> 2;
			_mm256_storeu_ps(std::bit_cast<float*>(ram_base + write_address + 1), 
				std::bit_cast<__m256>(block));
		}

		block_control->word_count %= 8;
	}
}