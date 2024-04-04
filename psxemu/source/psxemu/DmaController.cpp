#include <psxemu/include/psxemu/SystemStatus.hpp>
#include <psxemu/include/psxemu/DmaController.hpp>
#include <psxemu/include/psxemu/Interrupts.hpp>

#include <common/Errors.hpp>

#include <fmt/format.h>

#include <algorithm>

namespace psx {
	static constexpr u32 DPCR_INIT = 0x07654321;

	DmaController::DmaController(system_status* sys_status) :
		m_sys_status{ sys_status }, m_control{}, 
		m_int_control{}, m_ot_dma{sys_status, this}, 
		m_gpu_dma{sys_status, this},
		m_active_dmas{}, m_num_active{0} {
		m_control.raw = DPCR_INIT;
	}

	void DmaController::UpdateMasterIRQ() {
		u32 int_req = m_int_control.channel_int_req;
		u32 int_en = m_int_control.channel_int_enable;
		u32 anded = int_req & int_en;

		if (m_int_control.bus_error || (m_int_control.master_int_enable && anded != 0))
			m_int_control.master_irq = true;
		else
			m_int_control.master_irq = false;
	}

	void DmaController::Write(u32 address, u32 value, u32 mask) {
		if (address >= DMA_CONTROL && address < DMA_CONTROL + 4) {
			m_control.raw = value;
			fmt::println("[DMA CONTROLLER] DMA Control = 0x{:x}", m_control.raw);

			if (value != 0) {
				fmt::println("                 MDECin priority  =   {}", (u32)m_control.dma0_prio);
				fmt::println("                 MDECin enable    =   {}", (bool)m_control.dma0_en);
				fmt::println("                 MDECout priority =   {}", (u32)m_control.dma1_prio);
				fmt::println("                 MDECout enable   =   {}", (bool)m_control.dma1_en);
				fmt::println("                 GPU priority     =   {}", (u32)m_control.dma2_prio);
				fmt::println("                 GPU enable       =   {}", (bool)m_control.dma2_en);
				fmt::println("                 CDROM priority   =   {}", (u32)m_control.dma3_prio);
				fmt::println("                 CDROM enable     =   {}", (bool)m_control.dma3_en);
				fmt::println("                 SPU priority     =   {}", (u32)m_control.dma4_prio);
				fmt::println("                 SPU enable       =   {}", (bool)m_control.dma4_en);
				fmt::println("                 PIO priority     =   {}", (u32)m_control.dma5_prio);
				fmt::println("                 PIO enable       =   {}", (bool)m_control.dma5_en);
				fmt::println("                 OTC priority     =   {}", (u32)m_control.dma6_prio);
				fmt::println("                 OTC enable       =   {}", (bool)m_control.dma6_en);
			}

			return;
		}

		if (address >= DMA_INT && address < DMA_INT + 4) {
			bool master_irq = m_int_control.master_irq;
			u32 irq = m_int_control.channel_int_req;
			u32 ack = (value >> 24) & 0x7F;

			irq &= ~ack;

			m_int_control.raw = value;

			m_int_control.master_irq = master_irq;
			m_int_control.channel_int_req = (u8)irq;

			UpdateMasterIRQ();

			fmt::println("[DMA CONTROLLER] INT Control = 0x{:x}", m_int_control.raw);

			if (value != 0) {
				fmt::println("                 Completion int (1=for each block) = {:07b}", (u32)m_int_control.interrupt_on_block);
				fmt::println("                 Bus error                         = {}", (bool)m_int_control.bus_error);
				fmt::println("                 Interrupt enable                  = {:07b}", (u32)m_int_control.channel_int_enable);
				fmt::println("                 Interrupt flags                   = {:07b}", (u32)m_int_control.channel_int_req);
				fmt::println("                 Master interrupt enable           = {}", (bool)m_int_control.master_int_enable);
				fmt::println("                 Master interrupt flag             = {}", (bool)m_int_control.master_irq);
			}

			return;
		}

		if ((address & 0xF0) == DMA6_ADD) {
			m_ot_dma.Write(address - DMA6_ADD, value, mask);
			return;
		}

		if ((address & 0xF0) == DMA2_ADD) {
			m_gpu_dma.Write(address - DMA2_ADD, value, mask);
			return;
		}

		fmt::println("[DMA CONTROLLER] Accessing invalid/unused register 0x{:x}", address);
	}

	u32 DmaController::Read(u32 address) {
		if (address >= DMA_CONTROL && address < DMA_CONTROL + 4) {
			return m_control.raw;
		}

		if (address >= DMA_INT && address < DMA_INT + 4) {
			UpdateMasterIRQ();
			return m_int_control.raw;
		}

		if ((address & 0xF0) == DMA6_ADD) {
			return m_ot_dma.Read(address - DMA6_ADD);
		}

		if ((address & 0xF0) == DMA2_ADD) {
			return m_gpu_dma.Read(address - DMA2_ADD);
		}

		fmt::println("[DMA CONTROLLER] Accessing invalid/unused register 0x{:x}", address);
		return 0;
	}

	void DmaController::AddTransfer(u8 dma_id) {
		if (m_num_active == 7) {
			fmt::println("[DMA CONTROLLER] Too many transfers!");
			error::DebugBreak();
		}

		bool enabled = (bool)((m_control.raw >> (dma_id * 4 + 3)) & 1);

		if (!enabled)
			return;

		u8 curr_pos = m_num_active;
		u8 prio = (m_control.raw >> (4 * dma_id)) & 0x7;

		while (curr_pos) {
			if (m_active_dmas[curr_pos].dma_priority < prio)
				break;
			else if (m_active_dmas[curr_pos].dma_priority == prio
				&& m_active_dmas[curr_pos].dma_id > dma_id)
				break;

			curr_pos--;
		}

		std::shift_right(m_active_dmas + curr_pos, std::end(m_active_dmas), 1);

		m_active_dmas[curr_pos].dma_id = dma_id;
		m_active_dmas[curr_pos].dma_priority = prio;

		m_num_active++;
	}

	void DmaController::RemoveTransfer() {
		if (m_num_active == 0)
			return;

		std::shift_left(m_active_dmas, std::end(m_active_dmas), 1);

		m_num_active--;
	}

	void DmaController::AdvanceTransfer() {
		switch (m_active_dmas[0].dma_id)
		{
		case 0x2:
			m_gpu_dma.AdvanceTransfer();
			break;
		case 0x6:
			m_ot_dma.AdvanceTransfer();
			break;
		default:
			error::DebugBreak();
			break;
		}
	}

	void DmaController::SignalException() {
		m_int_control.bus_error = true;

		UpdateMasterIRQ();

		if (!m_int_control.master_irq) {
			m_int_control.master_irq = true;
			m_sys_status->Interrupt((u32)Interrupts::DMA);
		}
		
		m_sys_status->exception = false;
	}

	void DmaController::InterruptRequest(u8 dma_id) {
		UpdateMasterIRQ();

		if ((m_int_control.channel_int_enable >> dma_id) & 1) {
			m_int_control.channel_int_req |= (1 << dma_id);

			if (!m_int_control.master_irq) {
				m_int_control.master_irq = true;
				m_sys_status->Interrupt((u32)Interrupts::DMA);
			}
		}
	}
}