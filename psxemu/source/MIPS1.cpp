#include <psxemu/include/psxemu/MIPS1.hpp>
#include <psxemu/include/psxemu/SystemBus.hpp>
#include <psxemu/include/psxemu/Interpreter.hpp>

namespace psx::cpu {
	MIPS1::MIPS1(system_status* sys_status) :
		m_regs{}, m_pc{},
		m_hi{}, m_lo{},
		m_coprocessor0{}, 
		m_sys_status{sys_status} {}

	bool MIPS1::CheckInterrupts() {
		bool bit10 = !!((m_coprocessor0.registers.sr.int_mask >> 2) & 1);

		if (!m_coprocessor0.registers.sr.curr_int_enable || !bit10)
			return false;

		if((m_sys_status->interrupt_mask & m_sys_status->interrupt_request) == 0) {
			u8 sw_bits_cause = (m_coprocessor0.registers.cause.interrupt_pending & 3);
			u8 sw_bits_sr = (m_coprocessor0.registers.sr.int_mask & 3);

			if ((sw_bits_sr & sw_bits_cause) == 0)
				return false;
		}

		return true;
	}

	void MIPS1::FlushLoadDelay() {
		if (m_sys_status->load_delay_countdown == 0)
			return;

		m_sys_status->load_delay_countdown = 0;

		u32 load_delay_val = m_sys_status->delay_value;
		u8 load_delay_dest = m_sys_status->load_delay_dest;

		m_regs.array[load_delay_dest] = load_delay_val;
	}

	void MIPS1::StepInstruction() {
		auto bus = m_sys_status->sysbus;

		if (CheckInterrupts()) {
			m_sys_status->Exception(Excode::INT, false);
			m_sys_status->branch_delay = false;
		}

		bool in_bd = m_sys_status->branch_delay;
		u32 instruction = 0;

		u32 segment = m_pc & 0xF0000000;

		if (segment == 0xA0000000 || segment == 0xB0000000) {
			instruction = bus->Read<u32, true, true>(m_pc);
		}
		else {
			instruction = bus->Read<u32, true, false>(m_pc);
		}

		if(!m_sys_status->exception)
			cpu::InterpretMips(m_sys_status, instruction);

		bus->m_curr_cycles += 1;

		if (m_sys_status->exception) {
			m_sys_status->exception = false;
			m_sys_status->branch_delay = false;
			FlushLoadDelay();
			m_sys_status->Exception(m_sys_status->exception_number, false);
		}
		else if (in_bd) {
			m_sys_status->branch_delay = false;
			FlushLoadDelay();
			m_pc = m_sys_status->branch_dest;
		}
		else {
			if (m_sys_status->load_delay_countdown == 2)
				m_sys_status->load_delay_countdown -= 1;
			else if (m_sys_status->load_delay_countdown == 1)
				FlushLoadDelay();

			m_pc += 4;
		}
	}
}