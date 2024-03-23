#pragma once

#include <psxemu/include/psxemu/SystemBus.hpp>
#include <psxemu/include/psxemu/Interpreter.hpp>
#include <psxemu/include/psxemu/cpu_instruction.hpp>
#include <common/Errors.hpp>

#include <fmt/format.h>

namespace psx::cpu {
	FORCE_INLINE bool AddOverflow(u32 l, u32 r) {
		u32 sum = l + r;
		return ((~(l ^ r) & (l ^ sum)) & 0x80000000);
	}

	//LUI <reg>, <imm16> load immediate value to upper 16
	//bits of registers <reg>
	void LoadUpperImmediate(system_status* status, u32 instruction) {
		u32 dest_reg = (instruction >> 16) & 0x1F;

		status->cpu->FlushLoadDelay();

		if (dest_reg != 0) {
			u32 imm = (u16)instruction;

			u32& reg = status->cpu->GetRegs().array[dest_reg];

			//Zeroes lower bits
			reg = (imm << 16);
		}
	}

	void ReservedInstruction(system_status* status, u32 instruction) {
		u16 primary = (instruction >> 26) & 0x3F;
		u16 secondary = instruction & 0x3F;

		u16 hash = (primary << 6) | secondary;

		auto const& type = INSTRUCTION_TYPE_LUT[hash];

		if (std::get<1>(type) == Opcode::NA) {
#ifdef DEBUG_CPU_ERRORS
			fmt::println("Reserved instruction 0x{:x} at 0x{:x}", instruction,
				status->cpu->GetPc());
#endif // DEBUG_CPU_ERRORS

			status->exception = true;
			status->exception_number = Excode::RI;
		}
		else {
			fmt::println("Unimplemented instruction 0x{:x} at 0x{:x}", instruction,
				status->cpu->GetPc());

			error::DebugBreak();
		}
	}

	template <Opcode AluOpcode>
	void AluImmediate(system_status* status, u32 instruction) {
		u32 rs = (instruction >> 21) & 0x1F;
		u32 rt = (instruction >> 16) & 0x1F;
		u32 imm = (u16)instruction;

		auto& regs = status->cpu->GetRegs();

		u32 rs_val = regs.array[rs];

		status->cpu->FlushLoadDelay();

		if constexpr (AluOpcode == Opcode::ORI) {
			regs.array[rt] = rs_val | imm;
		}
		else if constexpr (AluOpcode == Opcode::ADDIU) {
			i32 imm_sign_extend = (i16)imm;
			regs.array[rt] = (u32)( rs_val + imm_sign_extend);
		}
		else if constexpr (AluOpcode == Opcode::ADDI) {
			i32 imm_sign_extend = (i16)imm;

			if (AddOverflow(rs_val, imm)) {
				status->Exception(Excode::OV, false);
			}
			else {
				regs.array[rt] = rs_val + imm_sign_extend;
			}
		}
		else if constexpr (AluOpcode == Opcode::ANDI) {
			regs.array[rt] = rs_val & imm;
		}
		else if constexpr (AluOpcode == Opcode::XORI) {
			regs.array[rt] = rs_val ^ imm;
		}
		else {
			fmt::println("Invalid/Unimplemented ALU Immediate Opcode 0x{:x}", (u8)AluOpcode);
			error::DebugBreak();
		}
	}

	template <Opcode ShiftOpcode>
	void ShiftImmediate(system_status* status, u32 instruction) {
		u32 rt = (instruction >> 16) & 0x1F;
		u32 rd = (instruction >> 11) & 0x1F;
		u32 imm = (instruction >> 6) & 0x1F;

		auto& regs = status->cpu->GetRegs();

		u32 rt_val = regs.array[rt];

		status->cpu->FlushLoadDelay();

		if (ShiftOpcode == Opcode::SLL) {
			regs.array[rd] = rt_val << imm;
		}
		else {
			error::DebugBreak();
		}
	}

	template <Opcode StoreOpcode>
	void Store(system_status* status, u32 instruction) {
		//Cache isolated -> Read/Writes target only data cache
		if (status->cpu->GetCOP0().registers.sr.isolate_cache)
			return;

		u32 rs = (instruction >> 21) & 0x1F;
		u32 rt = (instruction >> 16) & 0x1F;
		i32 off = (i16)(instruction & 0xFFFF);

		auto& regs = status->cpu->GetRegs();

		u32 base = (rs == 0) ? 0 : regs.array[rs];
		u32 value = (rt == 0) ? 0 : regs.array[rt];

		status->cpu->FlushLoadDelay();

		if constexpr (StoreOpcode == Opcode::SW) {
			//Perform direct write, do not emulate store
			//delay
			status->sysbus->Write<u32, true, true>((u32)(base + off), value);
		}
		else if constexpr (StoreOpcode == Opcode::SH) {
			status->sysbus->Write<u16, true, true>((u32)(base + off), (u16)value);
		}
		else if constexpr (StoreOpcode == Opcode::SB) {
			status->sysbus->Write<u8, true, true>((u32)(base + off), (u8)value);
		}
		else {
			error::DebugBreak();
		}
	}

	template <Opcode JumpOpcode>
	void Jump(system_status* status, u32 instruction) {
		//pc=(pc and F0000000h)+(imm26bit*4)
		status->branch_delay = true;

		u32 rs = (instruction >> 21) & 0x1F;

		u32 rs_val = status->cpu->GetRegs().array[rs];

		u32 curr_pc = status->cpu->GetPc();

		status->cpu->FlushLoadDelay();

		if constexpr (JumpOpcode == Opcode::J) {
			u32 offset = (instruction & 0x3FFFFFF) << 2;
			u32 base = curr_pc & 0xF0000000;
			status->Jump(base + offset);
		}
		else if constexpr (JumpOpcode == Opcode::JAL) {
			u32 offset = (instruction & 0x3FFFFFF) << 2;
			u32 base = curr_pc & 0xF0000000;
			status->cpu->GetRegs().ra = curr_pc + 8;
			status->Jump(base + offset);
		}
		else if constexpr (JumpOpcode == Opcode::JR) {
			status->Jump(rs_val);
		}
		else {
			error::DebugBreak();
		}
	}

	template <Opcode AluOpcode>
	void AluReg(system_status* status, u32 instruction) {
		u32 rs = (instruction >> 21) & 0x1F;
		u32 rt = (instruction >> 16) & 0x1F;
		u32 rd = (instruction >> 11) & 0x1F;

		auto& regs = status->cpu->GetRegs();

		u32 rs_val = regs.array[rs];
		u32 rt_val = regs.array[rt];

		status->cpu->FlushLoadDelay();

		if constexpr (AluOpcode == Opcode::OR) {
			regs.array[rd] = rs_val | rt_val;
		}
		else if constexpr (AluOpcode == Opcode::ADDU) {
			regs.array[rd] = rs_val + (i32)rt_val;
		}
		else if constexpr (AluOpcode == Opcode::ADD) {
			if (AddOverflow(rs_val, rt_val)) {
				status->Exception(Excode::OV, false);
				return;
			}

			regs.array[rd] = rs_val + (i32)rt_val;
		}
		else if constexpr (AluOpcode == Opcode::SLTU) {
			u32 l = rs_val;
			u32 r = rt_val;

			if (l < r)
				regs.array[rd] = 1;
			else
				regs.array[rd] = 0;
		}
		else if constexpr (AluOpcode == Opcode::SLT) {
			//Signed comparison
			i32 l = rs_val;
			i32 r = rt_val;

			if (l < r)
				regs.array[rd] = 1;
			else
				regs.array[rd] = 0;
		}
		else {
			error::DebugBreak();
		}
	}

	void Coprocessor0Command(system_status* status, u32 instruction) {
		u32 action = (instruction >> 21) & 0xF;

		u8 rt = (instruction >> 16) & 0x1F;
		u8 rd = (instruction >> 11) & 0x1F;

		u32 value = status->cpu->GetRegs().array[rt];

		status->cpu->FlushLoadDelay();

		switch (action)
		{
		case 0x0:
			status->cpu->ReadCOP0(rd, rt);
			break;
		case 0x2:
			status->Exception(Excode::RI, false);
			break;
		case 0x4: {
			
			status->cpu->WriteCOP0(value, rd);
		}
			break;
		case 0x6:
			status->Exception(Excode::RI, false);
			break;
		default:
			status->CoprocessorUnusableException(0);
			break;
		}
	}

	template <Opcode CopNumber>
	void CopCmd(system_status* status, u32 instruction) {
		if constexpr (CopNumber == Opcode::NA) {
			error::DebugBreak();
		}

		if constexpr (CopNumber == Opcode::COP0) {
			if ((instruction >> 25) & 1) {
				if ((instruction & 0x3F) == 0x10) {
					status->ExitException();
				}
				else {
					status->Exception(Excode::RI, false);
				}
			}
			else {
				Coprocessor0Command(status, instruction);
			}
		}
		else if constexpr (CopNumber == Opcode::COP2) {
			error::DebugBreak();
		}
		else {
			u8 coprocessor_num = CopNumber == Opcode::COP1 ? 1 : 3;
			status->CoprocessorUnusableException(coprocessor_num);
		}
	}

	template <Opcode BranchOpcode>
	void Branch(system_status* status, u32 instruction) {
		if constexpr (BranchOpcode == Opcode::NA) {
			error::DebugBreak();
		}

		status->branch_delay = true;

		u32 rs = (instruction >> 21) & 0x1F;
		u32 rt = (instruction >> 16) & 0x1F;
		i32 imm = (i16)instruction * 4;

		auto& regs = status->cpu->GetRegs();

		u32 pc_val = status->cpu->GetPc() + 4;

		u32 rs_val = regs.array[rs];
		u32 rt_val = regs.array[rt];

		status->cpu->FlushLoadDelay();

		if constexpr (BranchOpcode == Opcode::BNE) {
			u32 dest = (u32)(pc_val + imm);
			if (rs_val != rt_val)
				status->Jump(dest);
		}
		else if constexpr (BranchOpcode == Opcode::BEQ) {
			u32 dest = (u32)(pc_val + imm);
			if (rs_val == rt_val)
				status->Jump(dest);
		}
		else {
			error::DebugBreak();
		}
	}

	template <Opcode LoadOpcode>
	void Load(system_status* status, u32 instruction) {
		if constexpr (LoadOpcode == Opcode::NA) {
			error::DebugBreak();
		}

		u32 rs = (instruction >> 21) & 0x1F;
		u32 rt = (instruction >> 16) & 0x1F;
		i32 imm = (i16)instruction;

		if (rt == 0)
			return;

		auto& cpu = *status->cpu;

		u32 address = (u32)(cpu.GetRegs().array[rs] + imm);
		u32 value = 0;

		cpu.FlushLoadDelay();

		if constexpr (LoadOpcode == Opcode::LW) {
			value = status->sysbus->Read<u32, true, true>(address);
		}
		else if constexpr (LoadOpcode == Opcode::LHU) {
			value = status->sysbus->Read<u16, true, true>(address);
		}
		else if constexpr (LoadOpcode == Opcode::LBU) {
			value = status->sysbus->Read<u8, true, true>(address);
		}
		else if constexpr (LoadOpcode == Opcode::LH) {
			value = (u32)((i32)status->sysbus->Read<i16, true, true>(address));
		}
		else if constexpr (LoadOpcode == Opcode::LB) {
			value = (u32)((i32)status->sysbus->Read<i8, true, true>(address));
		}
		else {
			error::DebugBreak();
		}

		if (status->exception)
			return;

		status->AddLoadDelay(value, rt);
	}
}