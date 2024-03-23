#include <psxemu/include/psxemu/MIPS1.hpp>
#include <psxemu/include/psxemu/SystemBus.hpp>
#include <psxemu/include/psxemu/Interpreter.hpp>

#include <psxemu/include/psxemu/cpu_instruction.hpp>

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

	bool MIPS1::CheckInstructionGTE() {
		auto bus = m_sys_status->sysbus;

		u32 instruction = bus->Read<u32, false, false>(m_pc);

		u16 primary = (instruction >> 26) & 0x3F;
		u16 secondary = instruction & 0x3F;

		u16 hash = (primary << 6) | secondary;

		auto const& type = INSTRUCTION_TYPE_LUT[hash];

		return std::get<2>(type) == InstructionSubtype::COP_CMD &&
			std::get<1>(type) == Opcode::COP2;
	}

	void MIPS1::StepInstruction() {
		auto bus = m_sys_status->sysbus;

		if (CheckInterrupts()) {
			if (!CheckInstructionGTE()) {
				m_sys_status->Exception(Excode::INT, false);
				m_sys_status->branch_delay = false;
				m_sys_status->branch_taken = false;
			}
		}

		bool in_bd = m_sys_status->branch_delay;
		bool branch_taken = m_sys_status->branch_taken;
		u32 instruction = 0;

		u32 segment = m_pc & 0xF0000000;

		if ((segment == 0xA0000000 || segment == 0xB0000000) ||
			!bus->CacheEnabled()) {
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
			m_sys_status->branch_taken = false;
			FlushLoadDelay();
			m_sys_status->Exception(m_sys_status->exception_number, false);
		}
		else if (branch_taken) {
			m_sys_status->branch_delay = false;
			m_sys_status->branch_taken = false;
			FlushLoadDelay();
			m_pc = m_sys_status->branch_dest;
		}
		else {
			if (m_sys_status->load_delay_countdown == 2)
				m_sys_status->load_delay_countdown -= 1;
			else if (m_sys_status->load_delay_countdown == 1)
				FlushLoadDelay();

			if (in_bd)
				m_sys_status->branch_delay = false;

			m_pc += 4;
		}

		//Force reset ZERO register
		m_regs.zero = 0;
	}

	void MIPS1::ReadCOP0(u8 cop0_reg, u8 dest_reg) {
		FlushLoadDelay();

		u32 value = 0;

		if (cop0_reg >= 16 && cop0_reg < 32)
			value = 0xdeadbeef;
		else {
			if (m_coprocessor0.registers.sr.current_mode &&
				!m_coprocessor0.registers.sr.cop0_enable) {
				m_sys_status->Exception(Excode::COU, false);
				return;
			}

			switch (cop0_reg)
			{
			case 0x3:
				value = m_coprocessor0.registers.bpc;
				break;
			case 0x5:
				value = m_coprocessor0.registers.bda;
				break;
			case 0x6:
				value = m_coprocessor0.registers.jumpdest;
				break;
			case 0x7:
				value = m_coprocessor0.registers.dcic.reg;
				break;
			case 0x8:
				value = m_coprocessor0.registers.badvaddr;
				break;
			case 0x9:
				value = m_coprocessor0.registers.bdam;
				break;
			case 11:
				value = m_coprocessor0.registers.bpcm;
				break;
			case 12:
				value = m_coprocessor0.registers.sr.reg;
				break;
			case 13:
				value = m_coprocessor0.registers.cause.reg;
				if (m_sys_status->interrupt_mask & m_sys_status->interrupt_request) {
					value |= (1 << 10);
				}
				break;
			case 14:
				value = m_coprocessor0.registers.epc;
				break;
			case 15:
				value = m_coprocessor0.registers.prid;
				break;
			default:
				m_sys_status->Exception(Excode::RI, false);
				return;
			}
		}

		m_sys_status->AddLoadDelay(value, dest_reg);
	}

	void MIPS1::WriteCOP0(u32 value, u8 cop0_reg) {
		if (cop0_reg >= 16 && cop0_reg < 32)
			return;

		if (m_coprocessor0.registers.sr.current_mode &&
			!m_coprocessor0.registers.sr.cop0_enable) {
			m_sys_status->Exception(Excode::COU, false);
			return;
		}

		switch (cop0_reg)
		{
		case 3:
			m_coprocessor0.registers.bpc = value;
			break;
		case 5:
			m_coprocessor0.registers.bda = value;
			break;
		case 7:
			m_coprocessor0.registers.dcic.reg &= ~DCIC_WRITE_MASK;
			m_coprocessor0.registers.dcic.reg |= (value & DCIC_WRITE_MASK);
			break;
		case 9:
			m_coprocessor0.registers.bdam = value;
			break;
		case 11:
			m_coprocessor0.registers.bpcm = value;
			break;
		case 12:
			m_coprocessor0.registers.sr.reg &= ~SR_WRITE_MASK;
			m_coprocessor0.registers.sr.reg |= (value & SR_WRITE_MASK);
			break;
		case 13:
			m_coprocessor0.registers.cause.reg &= ~CAUSE_WRITE_MASK;
			m_coprocessor0.registers.cause.reg |= value & CAUSE_WRITE_MASK;
			break;
		case 6:
		case 8:
		case 14:
		case 15:
			return;
		default:
			m_sys_status->Exception(Excode::RI, false);
			return;
		}
	}
}