#include <psxemu/include/psxemu/MIPS1.hpp>
#include <psxemu/include/psxemu/SystemBus.hpp>
#include <psxemu/include/psxemu/Interpreter.hpp>
#include <psxemu/include/psxemu/cpu_instruction.hpp>

#include <common/Errors.hpp>

#include <psxemu/include/psxemu/Logger.hpp>
#include <psxemu/include/psxemu/LoggerMacros.hpp>

namespace psx::cpu {
	MIPS1::MIPS1(system_status* sys_status) :
		m_regs{}, m_pc{},
		m_hi{}, m_lo{},
		m_coprocessor0{}, 
		m_gte{sys_status},
		m_sys_status{sys_status} {
		m_sys_status->curr_delay.dest = InvalidReg;
		m_sys_status->next_delay.dest = InvalidReg;
		m_sys_status->reg_writeback.dest = InvalidReg;

		auto fake_entry = SyscallCallstackEntry{
			.exitpoint = UINT32_MAX,
			.syscall_id = UINT32_MAX,
			.caller = UINT32_MAX
		};

		//Just to make sure that at least one element
		//is always present
		m_syscall_frames.push(fake_entry);
	}

	bool MIPS1::CheckInterrupts() {
		if (!m_sys_status->interrupt_line)
			return false;

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

	void MIPS1::UpdateLoadDelay() {
		m_regs.array[m_sys_status->curr_delay.dest] = m_sys_status->curr_delay.value;
		m_sys_status->curr_delay = m_sys_status->next_delay;
		m_sys_status->next_delay.dest = InvalidReg;
	}

	void MIPS1::UpdateRegWriteback() {
		m_regs.array[m_sys_status->reg_writeback.dest] = m_sys_status->reg_writeback.value;
		m_sys_status->reg_writeback.dest = InvalidReg;
	}

	void MIPS1::FlushLoadDelay() {
		m_regs.array[m_sys_status->curr_delay.dest] = m_sys_status->curr_delay.value;
		m_sys_status->curr_delay.dest = InvalidReg;
		UpdateRegWriteback();
		m_regs.array[m_sys_status->next_delay.dest] = m_sys_status->next_delay.value;
		m_sys_status->next_delay.dest = InvalidReg;
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

		if (m_pc & 3) [[unlikely]] {
			LOG_ERROR("CPU", "[EXCPETION] Unalidgned PC {:#x}", m_pc);
			m_sys_status->Exception(Excode::ADEL, false);
			m_sys_status->branch_delay = false;
			m_sys_status->branch_taken = false;
		}

		if (CheckInterrupts()) {
			if (!CheckInstructionGTE()) {
				m_sys_status->interrupt_line = false;
				FlushLoadDelay();
				m_sys_status->Exception(Excode::INT, false);
				m_sys_status->branch_delay = false;
				m_sys_status->branch_taken = false;
			}
			else {
				error::DebugBreak();
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
			FlushLoadDelay();
			m_sys_status->Exception(m_sys_status->exception_number, false);
			m_sys_status->exception = false;
			m_sys_status->branch_delay = false;
			m_sys_status->branch_taken = false;
		}
		else if (branch_taken) {
			m_sys_status->branch_delay = false;
			m_sys_status->branch_taken = false;

			//FlushLoadDelay();

			UpdateLoadDelay();
			UpdateRegWriteback();

			if (HLE_Bios(m_sys_status->branch_dest))
				m_pc = m_sys_status->branch_dest;
			else
				m_pc = m_regs.ra;
		}
		else {
			UpdateLoadDelay();
			UpdateRegWriteback();

			if (in_bd)
				m_sys_status->branch_delay = false;

			m_pc += 4;
		}

		//Force reset ZERO register
		m_regs.zero = 0;
	}

	void MIPS1::ReadCOP0(u8 cop0_reg, u8 dest_reg) {
		u32 value = 0;

		if (cop0_reg >= 16 && cop0_reg < 32)
			value = 0xdeadbeef;
		else {
			if (m_coprocessor0.registers.sr.current_mode &&
				!m_coprocessor0.registers.sr.cop0_enable) {
				LOG_ERROR("CPU", "[EXCEPTION] Unusable COP0 in user mode at {:#x}",
					m_pc);
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
				LOG_ERROR("CPU", "[EXCEPTION] Unusable COP0 in user mode at {:#x}",
					m_pc);
				m_sys_status->Exception(Excode::RI, false);
				return;
			}
		}

		m_sys_status->AddLoadDelay(value, dest_reg);
	}

	u32 MIPS1::ReadCOP0(u8 cop0_reg) {
		u32 value = 0;

		if (cop0_reg >= 16 && cop0_reg < 32)
			value = 0xdeadbeef;
		else {
			if (m_coprocessor0.registers.sr.current_mode &&
				!m_coprocessor0.registers.sr.cop0_enable) {
				LOG_ERROR("CPU", "[EXCEPTION] Unusable COP0 in user mode at {:#x}",
					m_pc);
				m_sys_status->Exception(Excode::COU, false);
				return 0;
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
				LOG_ERROR("CPU", "[EXCEPTION] Unusable COP0 in user mode at {:#x}",
					m_pc);
				m_sys_status->Exception(Excode::RI, false);
				return 0;
			}
		}

		return value;
	}

	void MIPS1::WriteCOP2_Data(u32 value, u8 cop2_reg) {
		m_gte.WriteData(cop2_reg, value);
	}

	void MIPS1::ReadCOP2_Data(u8 cop2_reg, u8 dest_reg) {
		m_sys_status->AddLoadDelay(m_gte.ReadData(cop2_reg), dest_reg);
	}

	u32 MIPS1::ReadCOP2_Data(u8 cop2_reg) {
		return m_gte.ReadData(cop2_reg);
	}

	void MIPS1::WriteCOP2_Control(u32 value, u8 cop2_reg) {
		m_gte.WriteControl(cop2_reg, value);
	}

	void MIPS1::ReadCOP2_Control(u8 cop2_reg, u8 dest_reg) {
		m_sys_status->AddLoadDelay(m_gte.ReadControl(cop2_reg), dest_reg);
	}

	u32 MIPS1::ReadCOP2_Control(u8 cop2_reg) {
		return m_gte.ReadControl(cop2_reg);
	}

	void MIPS1::COP2Cmd(u32 cmd) {
		m_gte.Cmd(cmd);
	}

	void MIPS1::WriteCOP0(u32 value, u8 cop0_reg) {
		if (cop0_reg >= 16 && cop0_reg < 32)
			return;

		if (m_coprocessor0.registers.sr.current_mode &&
			!m_coprocessor0.registers.sr.cop0_enable) {
			LOG_ERROR("CPU", "[EXCEPTION] Unusable COP0 in user mode at {:#x}",
				m_pc);
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
			LOG_ERROR("CPU", "[EXCEPTION] Unusable COP0 in user mode at {:#x}",
				m_pc);
			m_sys_status->Exception(Excode::RI, false);
			return;
		}
	}

	bool MIPS1::HLE_Bios(u32 address) {
		address &= 0x1FFFFFFF;

		switch (address)
		{
		case 0xA0:
		case 0xB0:
		case 0xC0: {
			bool should_enter = m_hle_bios_handler(address, true);
			if (should_enter) {
				if (m_syscall_frames.stacktop() == MAX_SYSCALL_FRAMES) [[unlikely]] {
					LOG_ERROR("CPU", "[CPU] FATAL! Max recursion reached for syscall frames!");
					error::DebugBreak();
				}
				u32 r9 = m_regs.array[9];
				u32 function_id = (address << 4) | r9;

				//ReturnFromException is [[noreturn]]
				//Better not put it in the frame pointer stack
				if (function_id != 0xb17) {
					m_syscall_frames.push(
						SyscallCallstackEntry{
							.exitpoint = m_regs.ra & 0x1FFFFFFF,
							.syscall_id = function_id,
							.caller = m_pc - 0x4
						}
					);
				}
			}
			return should_enter;
			}
			break;
		default:
			break;
		}

		if (m_syscall_frames.top().exitpoint == address) {
			do {
				SyscallCallstackEntry entry = m_syscall_frames.pop();
				
				if (entry.exitpoint == UINT32_MAX) [[unlikely]] {
					LOG_ERROR("CPU", "[CPU] \"Impossible\" happened! Popped invalid entry from system call frame pointer stack!");
					error::DebugBreak();
				}

				m_hle_bios_handler(entry.syscall_id, false);
			} while (m_syscall_frames.top().exitpoint == address);
		}

		return true;
	}

	void MIPS1::InterlockHiLo() {
		auto& sysbus = m_sys_status->sysbus;

		auto curr_timestamp = m_sys_status->scheduler.GetTimestamp();
		auto hi_lo_ready_timestamp = m_sys_status->hi_lo_ready_timestamp;

		if (curr_timestamp < hi_lo_ready_timestamp)
			sysbus->m_curr_cycles += hi_lo_ready_timestamp -
			curr_timestamp;
	}
}