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

		if (dest_reg != 0) {
			u32 imm = (u16)instruction;

			//Zeroes lower bits
			status->AddWriteback(imm << 16, (u8)dest_reg);
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

		if constexpr (AluOpcode == Opcode::ORI) {
			status->AddWriteback(rs_val | imm, rt);
		}
		else if constexpr (AluOpcode == Opcode::ADDIU) {
			i32 imm_sign_extend = (i16)imm;
			status->AddWriteback((u32)(rs_val + imm_sign_extend), rt);
		}
		else if constexpr (AluOpcode == Opcode::ADDI) {
			i32 imm_sign_extend = (i16)imm;

			if (AddOverflow(rs_val, imm)) {
				status->Exception(Excode::OV, false);
				return;
			}
			
			status->AddWriteback((u32)(rs_val + imm_sign_extend), rt);
		}
		else if constexpr (AluOpcode == Opcode::ANDI) {
			status->AddWriteback(rs_val & imm, rt);
		}
		else if constexpr (AluOpcode == Opcode::XORI) {
			status->AddWriteback(rs_val ^ imm, rt);
		}
		else if constexpr (AluOpcode == Opcode::SLTI) {
			i32 se_imm = (i16)imm;

			if ((i32)rs_val < se_imm)
				status->AddWriteback(1, rt);
			else
				status->AddWriteback(0, rt);
		}
		else if constexpr (AluOpcode == Opcode::SLTIU) {
			i32 se_imm = (i16)imm;

			if(rs_val < (u32)se_imm)
				status->AddWriteback(1, rt);
			else
				status->AddWriteback(0, rt);
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

		if (ShiftOpcode == Opcode::SLL) {
			status->AddWriteback(rt_val << imm, rd);
		}
		else if constexpr (ShiftOpcode == Opcode::SRA) {
			i32 rt_se = (i32)rt_val;
			rt_se >>= imm;

			status->AddWriteback(rt_se, rd);
		}
		else if constexpr (ShiftOpcode == Opcode::SRL) {
			rt_val >>= imm;

			status->AddWriteback(rt_val, rd);
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
		else if constexpr (StoreOpcode == Opcode::SWL) {
			u32 address = (u32)(base + off);

			u32 value_at = status->sysbus
				->Read<u32, true, true>(address & ~3);
			u32 to_write = 0;

			switch (address % 4)
			{
			case 0x0:
				to_write = (value_at & 0x00'FF'FF'FF) | (value & 0xFF'00'00'00);
				break;
			case 0x1:
				to_write = (value_at & 0x00'00'FF'FF) | (value & 0xFF'FF'00'00);
				break;
			case 0x2:
				to_write = (value_at & 0x00'00'00'FF) | (value & 0xFF'FF'FF'00);
				break;
			case 0x3:
				to_write = value;
				break;
			default:
				break;
			}

			/*switch (address % 4)
			{
			case 0x3:
				to_write = (value_at & 0x00'FF'FF'FF) | (value & 0xFF'00'00'00);
				break;
			case 0x2:
				to_write = (value_at & 0x00'00'FF'FF) | (value & 0xFF'FF'00'00);
				break;
			case 0x1:
				to_write = (value_at & 0x00'00'00'FF) | (value & 0xFF'FF'FF'00);
				break;
			case 0x0:
				to_write = value;
				break;
			default:
				break;
			}*/

			status->sysbus->Write<u32, true, true>(address & ~3, to_write);
		}
		else if constexpr (StoreOpcode == Opcode::SWR) {
			u32 address = (u32)(base + off);

			u32 value_at = status->sysbus
				->Read<u32, true, true>(address & ~3);
			u32 to_write = 0;

			switch (address % 4)
			{
			case 0x0:
				to_write = value;
				break;
			case 0x1:
				to_write = (value_at & 0xFF'00'00'00) | (value & 0x00'FF'FF'FF);
				break;
			case 0x2:
				to_write = (value_at & 0xFF'FF'00'00) | (value & 0x00'00'FF'FF);
				break;
			case 0x3:
				to_write = (value_at & 0xFF'FF'FF'00) | (value & 0x00'00'00'FF);
				break;
			default:
				break;
			}

			/*switch (address % 4)
			{
			case 0x3:
				to_write = value;
				break;
			case 0x2:
				to_write = (value_at & 0xFF'00'00'00) | (value & 0x00'FF'FF'FF);
				break;
			case 0x1:
				to_write = (value_at & 0xFF'FF'00'00) | (value & 0x00'00'FF'FF);
				break;
			case 0x0:
				to_write = (value_at & 0xFF'FF'FF'00) | (value & 0x00'00'00'FF);
				break;
			default:
				break;
			}*/

			status->sysbus->Write<u32, true, true>(address & ~3, to_write);
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
		u32 rd = (instruction >> 11) & 0x1F;

		u32 rs_val = status->cpu->GetRegs().array[rs];

		u32 curr_pc = status->cpu->GetPc();

		if constexpr (JumpOpcode == Opcode::J) {
			u32 offset = (instruction & 0x3FFFFFF) << 2;
			u32 base = curr_pc & 0xF0000000;
			status->Jump(base + offset);
		}
		else if constexpr (JumpOpcode == Opcode::JAL) {
			u32 offset = (instruction & 0x3FFFFFF) << 2;
			u32 base = curr_pc & 0xF0000000;

			status->AddWriteback(curr_pc + 8, 31);
			status->Jump(base + offset);
		}
		else if constexpr (JumpOpcode == Opcode::JR) {
			status->Jump(rs_val);
		}
		else if constexpr (JumpOpcode == Opcode::JALR) {
			status->AddWriteback(curr_pc + 8, rd);
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

		if constexpr (AluOpcode == Opcode::OR) {
			status->AddWriteback(rs_val | rt_val, rd);
		}
		else if constexpr (AluOpcode == Opcode::AND) {
			status->AddWriteback(rs_val & rt_val, rd);
		}
		else if constexpr (AluOpcode == Opcode::ADDU) {
			status->AddWriteback(rs_val + (i32)rt_val, rd);
		}
		else if constexpr (AluOpcode == Opcode::ADD) {
			if (AddOverflow(rs_val, rt_val)) {
				status->Exception(Excode::OV, false);
				return;
			}

			status->AddWriteback(rs_val + (i32)rt_val, rd);
		}
		else if constexpr (AluOpcode == Opcode::SLTU) {
			u32 l = rs_val;
			u32 r = rt_val;

			if (l < r)
				status->AddWriteback(1, rd);
			else
				status->AddWriteback(0, rd);
		}
		else if constexpr (AluOpcode == Opcode::SLT) {
			//Signed comparison
			i32 l = rs_val;
			i32 r = rt_val;

			if (l < r)
				status->AddWriteback(1, rd);
			else
				status->AddWriteback(0, rd);
		}
		else if constexpr (AluOpcode == Opcode::SUBU) {
			status->AddWriteback(rs_val - rt_val, rd);
		}
		else if constexpr (AluOpcode == Opcode::NOR) {
			constexpr u32 CONSTANT = 0xFFFFFFFF;
			status->AddWriteback(CONSTANT ^ (rs_val | rt_val), rd);
		}
		else if constexpr (AluOpcode == Opcode::XOR) {
			status->AddWriteback(rs_val ^ rt_val, rd);
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
			fmt::println("COP2 opcode {:#x}", instruction);
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
		else if constexpr (BranchOpcode == Opcode::BLEZ) {
			u32 dest = (u32)(pc_val + imm);
			if ((i32)rs_val <= 0)
				status->Jump(dest);
		}
		else if constexpr (BranchOpcode == Opcode::BGTZ) {
			u32 dest = (u32)(pc_val + imm);
			if ((i32)rs_val > 0)
				status->Jump(dest);
		}
		else if constexpr (BranchOpcode == Opcode::BCONDZ) {
			u32 type = (instruction >> 16) & 0x1F;
			u32 dest = (u32)(pc_val + imm);

			switch (type)
			{
			case 0x0: //BLTZ
				if ((i32)rs_val < 0)
					status->Jump(dest);
				break;
			case 0x1: //BGEZ
				if ((i32)rs_val >= 0)
					status->Jump(dest);
				break;
			case 0x10: //BLTZAL
				if ((i32)rs_val < 0) {
					status->Jump(dest);
				}
				status->AddWriteback(pc_val + 4, 31);
				break;
			case 0x11: //BGEZAL
				if ((i32)rs_val >= 0) {
					status->Jump(dest);
				}
				status->AddWriteback(pc_val + 4, 31);
				break;
			default:
				fmt::println("Invalid BCONDZ 0x{:x}, ignore", type);
				break;
			}
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
		else if constexpr (LoadOpcode == Opcode::LWL) {
			if (status->curr_delay.dest == rt) {
				cpu.GetRegs()
					.array[rt] = status->curr_delay.value;
				status->curr_delay.dest = InvalidReg;
			}

			u32 data = status->sysbus
				->Read<u32, false, true>(address & ~3);

			u32 rt_val = status->cpu->GetRegs()
				.array[rt];

			switch (address % 4)
			{
			case 0x0:
				value = (rt_val & 0x00'FF'FF'FF) | (data << 24);
				break;
			case 0x1:
				value = (rt_val & 0x00'00'FF'FF) | (data << 16);
				break;
			case 0x2:
				value = (rt_val & 0x00'00'00'FF) | (data << 8);
				break;
			case 0x3:
				value = data;
				break;
			default:
				break;
			}

			/*switch (address % 4)
			{
			case 0x3:
				value = (rt_val & 0x00'FF'FF'FF) | (data & 0xFF'00'00'00);
				break;
			case 0x2:
				value = (rt_val & 0x00'00'FF'FF) | (data & 0xFF'FF'00'00);
				break;
			case 0x1:
				value = (rt_val & 0x00'00'00'FF) | (data & 0xFF'FF'FF'00);
				break;
			case 0x0:
				value = data;
				break;
			default:
				break;
			}*/
		}
		else if constexpr (LoadOpcode == Opcode::LWR) {
			if (status->curr_delay.dest == rt) {
				cpu.GetRegs()
					.array[rt] = status->curr_delay.value;
				status->curr_delay.dest = InvalidReg;
			}

			u32 data = status->sysbus
				->Read<u32, false, true>(address & ~3);

			u32 rt_val = status->cpu->GetRegs()
				.array[rt];

			switch (address % 4)
			{
			case 0x0: 
				value = data;
				break;
			case 0x1:
				value = (rt_val & 0xFF'00'00'00) | (data >> 8);
				break;
			case 0x2:
				value = (rt_val & 0xFF'FF'00'00) | (data >> 16);
				break;
			case 0x3:
				value = (rt_val & 0xFF'FF'FF'00) | (data >> 24);
				break;
			default:
				break;
			}

			/*switch (address % 4)
			{
			case 0x3:
				value = data;
				break;
			case 0x2:
				value = (rt_val & 0xFF'00'00'00) | (data & 0x00'FF'FF'FF);
				break;
			case 0x1:
				value = (rt_val & 0xFF'FF'00'00) | (data & 0x00'00'FF'FF);
				break;
			case 0x0:
				value = (rt_val & 0xFF'FF'FF'00) | (data & 0x00'00'00'FF);
				break;
			default:
				break;
			}*/
		}
		else {
			error::DebugBreak();
		}

		if (status->exception)
			return;

		
		status->AddLoadDelay(value, rt);
	}

	template <Opcode MulDivOpcode>
	void MulDiv(system_status* status, u32 instruction) {
		if constexpr (MulDivOpcode == Opcode::NA) {
			error::DebugBreak();
		}

		u32 rs = (instruction >> 21) & 0x1F;
		u32 rt = (instruction >> 16) & 0x1F;
		u32 rd = (instruction >> 11) & 0x1F;

		auto& regs = status->cpu->GetRegs();

		u32 rs_val = regs.array[rs];
		u32 rt_val = regs.array[rt];
		u32 rd_val = regs.array[rd];

		if constexpr (MulDivOpcode == Opcode::DIV) {
			status->hi_lo_ready_timestamp = status->scheduler.GetTimestamp()
				+ 36;

			if (rt_val == 0x0) [[unlikely]] {
				if (rs_val >= 0) {
					status->cpu->GetHI() = rs_val;
					status->cpu->GetLO() = -1;
				}
				else {
					status->cpu->GetHI() = rs_val;
					status->cpu->GetLO() = 1;
				}
			} else if((i32)rt_val == -1 &&
				(i32)rs_val == -(i32)0x80000000) [[unlikely]] {
					status->cpu->GetHI() = 0x0;
					status->cpu->GetLO() = -(i32)0x80000000;
			}
			else {
				status->cpu->GetHI() = (i32)rs_val % (i32)rt_val;
				status->cpu->GetLO() = (i32)rs_val / (i32)rt_val;
			}
		}
		else if constexpr (MulDivOpcode == Opcode::MFHI) {
			status->cpu->InterlockHiLo();
			status->AddWriteback(status->cpu->GetHI(), rd);
		}
		else if constexpr (MulDivOpcode == Opcode::MFLO) {
			status->cpu->InterlockHiLo();
			status->AddWriteback(status->cpu->GetLO(), rd);
		} 
		else if constexpr (MulDivOpcode == Opcode::MTHI) {
			status->cpu->InterlockHiLo();
			status->cpu->GetHI() = rs_val;
		}
		else if constexpr (MulDivOpcode == Opcode::MTLO) {
			status->cpu->InterlockHiLo();
			status->cpu->GetLO() = rs_val;
		}
		else if constexpr (MulDivOpcode == Opcode::DIVU) {
			status->hi_lo_ready_timestamp = status->scheduler.GetTimestamp()
				+ 36;

			if (rt_val == 0) {
				status->cpu->GetLO() = 0xFFFFFFFF;
				status->cpu->GetHI() = rs_val;
			}
			else {
				status->cpu->GetHI() = rs_val % rt_val;
				status->cpu->GetLO() = rs_val / rt_val;
			}
		}
		else if constexpr (MulDivOpcode == Opcode::MULTU) {
			status->hi_lo_ready_timestamp = status->scheduler.GetTimestamp()
				+ 9;

			u64 res = (u64)rs_val * (u64)rt_val;

			status->cpu->GetLO() = (u32)res;
			status->cpu->GetHI() = (u32)(res >> 32);
		}
		else {
			error::DebugBreak();
		}
	}

	template <Opcode SysBreakOpcode>
	void SysBreak(system_status* status, u32 instruction) {
		if constexpr (SysBreakOpcode == Opcode::SYSCALL) {
			fmt::println("[EXCEPTION] SYSCALL Opcode at 0x{:x}",
				status->cpu->GetPc());

			u32 comment = status->cpu->GetRegs().a0;

			switch (comment)
			{
			case 0x0:
				fmt::println("[EXCEPTION] SYSCALL::NoFunction()");
				break;
			case 0x1:
				fmt::println("[EXCEPTION] SYSCALL::EnterCriticalSection()");
				break;
			case 0x2:
				fmt::println("[EXCEPTION] SYSCALL::ExitCriticalSection()");
				break;
			case 0x3: {
				u32 addr = status->cpu->GetRegs().a1;
				fmt::println("[EXCEPTION] SYSCALL::ChangeThreadSubFunction(addr=0x{:x})", 
					addr);
			}
				break;
			default:
				fmt::println("[EXCEPTION] SYSCALL::DeliverEvent(0xF0000010,0x4000)");
				break;
			}

			status->exception = true;
			status->exception_number = Excode::SYSCALL;
		}
		else if constexpr (SysBreakOpcode == Opcode::BREAK) {
			u32 comment = (instruction >> 6) & 0xFFFFF;
			fmt::println("[EXCEPTION] BREAK Opcode at 0x{:x} with arg 0x{:x}",
				status->cpu->GetPc(), comment);
			status->exception = true;
			status->exception_number = Excode::BP;
		}
	}

	template <Opcode ShiftOpcode>
	void ShiftReg(system_status* status, u32 instruction) {
		u32 rs = (instruction >> 21) & 0x1F;
		u32 rt = (instruction >> 16) & 0x1F;
		u32 rd = (instruction >> 11) & 0x1F;

		auto& regs = status->cpu->GetRegs();

		u32 rs_val = regs.array[rs];
		u32 rt_val = regs.array[rt];

		if constexpr (ShiftOpcode == Opcode::SLLV) {
			status->AddWriteback(rt_val << (rs_val & 0x1F), rd);
		}
		else if constexpr (ShiftOpcode == Opcode::SRLV) {
			status->AddWriteback(rt_val >> (rs_val & 0x1F), rd);
		}
		else if constexpr (ShiftOpcode == Opcode::SRAV) {
			i32 rt_se = (i32)rt_val;
			status->AddWriteback(rt_se >> (rs_val & 0x1F), rd);
		}
		else {
			error::DebugBreak();
		}
	}
}