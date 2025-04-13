#pragma once

#include <psxemu/include/psxemu/SystemBus.hpp>
#include <psxemu/include/psxemu/Interpreter.hpp>
#include <psxemu/include/psxemu/cpu_instruction.hpp>
#include <common/Errors.hpp>

#include <psxemu/include/psxemu/Logger.hpp>
#include <psxemu/include/psxemu/LoggerMacros.hpp>

#include <fmt/format.h>

namespace psx::cpu {
	FORCE_INLINE bool AddOverflow(u32 l, u32 r) {
		u32 sum = l + r;
		return ((~(l ^ r) & (l ^ sum)) & 0x80000000);
	}

	FORCE_INLINE bool SubOverflow(u32 l, u32 r) {
		u32 res = l - r;
		return ((l ^ r) & (l ^ res)) >> 31;
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

			LOG_ERROR("CPU", "[EXCEPTION] Reserved instruction 0x{:x} at 0x{:x}", instruction,
				status->cpu->GetPc());

			status->exception = true;
			status->exception_number = Excode::RI;
		}
		else {
			LOG_ERROR("CPU", "[EXCEPTION] Unimplemented instruction 0x{:x} at 0x{:x}", instruction,
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

			if (AddOverflow(rs_val, imm_sign_extend)) {
				status->exception = true;
				status->exception_number = Excode::OV;
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
			LOG_ERROR("CPU", "[EXCEPTION] Invalid/Unimplemented ALU Immediate Opcode 0x{:#x}", 
				(u8)AluOpcode);
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
			LOG_ERROR("CPU", "[EXCEPTION] Invalid/Unimplemented ALU Shift imm Opcode 0x{:#x}",
				(u8)ShiftOpcode);
			error::DebugBreak();
		}
	}

	template <Opcode StoreOpcode>
	void Store(system_status* status, u32 instruction) {
		//Cache isolated -> Read/Writes target only cache
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
				to_write = (value_at & 0xFF'FF'FF'00) | ((value >> 24) & 0xFF);
				break;
			case 0x1:
				to_write = (value_at & 0xFF'FF'00'00) | ((value >> 16) & 0xFFFF);
				break;
			case 0x2:
				to_write = (value_at & 0xFF'00'00'00) | ((value >> 8) & 0xFFFFFF);
				break;
			case 0x3:
				to_write = value;
				break;
			default:
				break;
			}

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
				to_write = (value_at & 0x00'00'00'FF) | ((value & 0x00'FF'FF'FF) << 8);
				break;
			case 0x2:
				to_write = (value_at & 0x00'00'FF'FF) | ((value & 0x00'00'FF'FF) << 16);
				break;
			case 0x3:
				to_write = (value_at & 0x00'FF'FF'FF) | ((value & 0x00'00'00'FF) << 24);
				break;
			default:
				break;
			}

			status->sysbus->Write<u32, true, true>(address & ~3, to_write);
		}
		else {
			LOG_ERROR("CPU", "[EXCEPTION] Invalid/Unimplemented STORE Opcode 0x{:#x}",
				(u8)StoreOpcode);
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
			LOG_ERROR("CPU", "[EXCEPTION] Invalid/Unimplemented JUMP Opcode 0x{:#x}",
				(u8)JumpOpcode);
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
				status->exception = true;
				status->exception_number = Excode::OV;
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
		else if constexpr (AluOpcode == Opcode::SUB) {
			if (SubOverflow(rs_val, rt_val)) {
				status->exception = true;
				status->exception_number = Excode::OV;
				return;
			}

			i32 res = i32(rs_val) - i32(rt_val);
			status->AddWriteback(u32(res), rd);
		}
		else if constexpr (AluOpcode == Opcode::NOR) {
			constexpr u32 CONSTANT = 0xFFFFFFFF;
			status->AddWriteback(CONSTANT ^ (rs_val | rt_val), rd);
		}
		else if constexpr (AluOpcode == Opcode::XOR) {
			status->AddWriteback(rs_val ^ rt_val, rd);
		}
		else {
			LOG_ERROR("CPU", "[EXCEPTION] Invalid/Unimplemented ALU REG Opcode 0x{:#x}",
				(u8)AluOpcode);
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
			LOG_ERROR("CPU", "[EXCEPTION] Invalid COP0 opcode 0x2 at {:#x}", 
				status->cpu->GetPc());
			status->Exception(Excode::RI, false);
			break;
		case 0x4: {
			
			status->cpu->WriteCOP0(value, rd);
		}
			break;
		case 0x6:
			LOG_ERROR("CPU", "[EXCEPTION] Invalid COP0 opcode 0x6 at {:#x}",
				status->cpu->GetPc());
			status->Exception(Excode::RI, false);
			break;
		default:
			LOG_ERROR("CPU", "[EXCEPTION] Invalid COP0 opcode {:#x} at {:#x}",
				action, status->cpu->GetPc());
			status->CoprocessorUnusableException(0);
			break;
		}
	}

//#pragma optimize("", off)
	void Coprocessor2Command(system_status* status, u32 instruction) {
		u32 action = (instruction >> 21) & 0xF;

		u8 rt = (instruction >> 16) & 0x1F;
		u8 rd = (instruction >> 11) & 0x1F;

		u32 value = status->cpu->GetRegs().array[rt];

		switch (action)
		{
		case 0x0:
			status->cpu->ReadCOP2_Data(rd, rt);
			break;
		case 0x2:
			status->cpu->ReadCOP2_Control(rd, rt);
			break;
		case 0x4:
			status->cpu->WriteCOP2_Data(value, rd);
			break;
		case 0x6:
			status->cpu->WriteCOP2_Control(value, rd);
			break;
		case 0x8:
			LOG_ERROR("CPU", "[CPU] Unimplemented COP2 jump if true/false at {:#010x}", 
				status->cpu->GetPc());
			error::DebugBreak();
			break;
		default:
			LOG_ERROR("CPU", "[EXCEPTION] Invalid COP2 opcode {:#x} at {:#x}",
				action, status->cpu->GetPc());
			status->CoprocessorUnusableException(2);
			break;
		}
	}

	template <Opcode CopNumber>
	void CopCmd(system_status* status, u32 instruction) {
		if constexpr (CopNumber == Opcode::NA) {
			LOG_ERROR("CPU", "[EXCEPTION] Invalid coprocessor {:#x}",
				CopNumber);
			error::DebugBreak();
		}

		if constexpr (CopNumber == Opcode::COP0) {
			if ((instruction >> 25) & 1) {
				if ((instruction & 0x3F) == 0x10) {
					status->ExitException();
				}
				else {
					LOG_ERROR("CPU", "[EXCEPTION] Invalid COP0 instruction {:#x} at {:#x}",
						instruction, status->cpu->GetPc());
					status->Exception(Excode::RI, false);
				}
			}
			else {
				Coprocessor0Command(status, instruction);
			}
		}
		else if constexpr (CopNumber == Opcode::COP2) {
			//LOG_DEBUG("CPU", "[COP2] Opcode {:#x}", instruction);

			auto enable_gte = status->cpu->GetCOP0().registers.sr.cop2_enable;
			if (!enable_gte) {
				status->CoprocessorUnusableException(2);
				return;
			}

			if ((instruction >> 25) & 1) {
				auto cmd = (instruction & 0x1FFFFFF);
				status->cpu->COP2Cmd(cmd);
			}
			else {
				Coprocessor2Command(status, instruction);
			}
			
		}
		else {
			LOG_ERROR("CPU", "[EXCEPTION] Invalid coprocessor {:#x} at {:#x}",
				u32(CopNumber), status->cpu->GetPc());
			u8 coprocessor_num = CopNumber == Opcode::COP1 ? 1 : 3;
			status->CoprocessorUnusableException(coprocessor_num);
		}
	}
//#pragma optimize("", on)

	template <Opcode BranchOpcode>
	void Branch(system_status* status, u32 instruction) {
		if constexpr (BranchOpcode == Opcode::NA) {
			LOG_ERROR("CPU", "[EXCEPTION] Invalid branch opcode {:#x} at {:#x}",
				u32(BranchOpcode), status->cpu->GetPc());
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

			type &= 0b10001;

			switch (type)
			{
			case 0x0: //BLTZ
			case 0x2: //BLTZL
				if ((i32)rs_val < 0)
					status->Jump(dest);
				break;
			case 0x1: //BGEZ
				if ((i32)rs_val >= 0)
					status->Jump(dest);
				break;
			case 0x10: //BLTZAL
			case 0x12: //BLTZALL
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
				LOG_INFO("CPU", "[OPCODE] Invalid BCONDZ 0x{:x}, ignore", type);
				break;
			}
		}
		else {
			LOG_ERROR("CPU", "[EXCEPTION] Invalid branch opcode {:#x} at {:#x}",
				u32(BranchOpcode), status->cpu->GetPc());
			error::DebugBreak();
		}
	}

	template <Opcode LoadOpcode>
	void Load(system_status* status, u32 instruction) {
		if constexpr (LoadOpcode == Opcode::NA) {
			LOG_ERROR("CPU", "[EXCEPTION] Invalid load opcode {:#x} at {:#x}",
				u32(LoadOpcode), status->cpu->GetPc());
			error::DebugBreak();
		}

		u32 rs = (instruction >> 21) & 0x1F;
		u32 rt = (instruction >> 16) & 0x1F;
		i32 imm = (i16)instruction;

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
			u32 rt_val = status->cpu->GetRegs()
				.array[rt];

			if (status->curr_delay.dest == rt) {
				rt_val = status->curr_delay.value;
				status->curr_delay.dest = InvalidReg;
			}

			u32 data = status->sysbus
				->Read<u32, false, true>(address & ~3);
			

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
		}
		else if constexpr (LoadOpcode == Opcode::LWR) {
			u32 rt_val = status->cpu->GetRegs()
				.array[rt];

			if (status->curr_delay.dest == rt) {
				rt_val = status->curr_delay.value;
				status->curr_delay.dest = InvalidReg;
			}

			u32 data = status->sysbus
				->Read<u32, false, true>(address & ~3);

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
		}
		else {
			LOG_ERROR("CPU", "[EXCEPTION] Invalid load opcode {:#x} at {:#x}",
				u32(LoadOpcode), status->cpu->GetPc());
			error::DebugBreak();
		}

		if (status->exception || rt == 0)
			return;

		if (status->curr_delay.dest == rt)
			status->curr_delay.dest = InvalidReg;
			
		status->AddLoadDelay(value, rt);
	}

	template <Opcode MulDivOpcode>
	void MulDiv(system_status* status, u32 instruction) {
		if constexpr (MulDivOpcode == Opcode::NA) {
			LOG_ERROR("CPU", "[EXCEPTION] Invalid muldiv opcode {:#x} at {:#x}",
				u32(MulDivOpcode), status->cpu->GetPc());
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
				if (i32(rs_val) >= 0) {
					status->cpu->GetHI() = rs_val;
					status->cpu->GetLO() = -1;
				}
				else {
					status->cpu->GetHI() = rs_val;
					status->cpu->GetLO() = 1;
				}
			} else if((i32)rt_val == -1 &&
				      rs_val == 0x80000000) [[unlikely]] {
					status->cpu->GetHI() = 0x0;
					status->cpu->GetLO() = 0x80000000;
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
		else if constexpr (MulDivOpcode == Opcode::MULT) {
			status->hi_lo_ready_timestamp = status->scheduler.GetTimestamp()
				+ 9;

			u64 res = u64(i64(i32(rs_val)) * i32(rt_val));

			status->cpu->GetLO() = (u32)res;
			status->cpu->GetHI() = (u32)(res >> 32);
		}
		else {
			LOG_ERROR("CPU", "[EXCEPTION] Invalid muldiv opcode {:#x} at {:#x}",
				u32(MulDivOpcode), status->cpu->GetPc());
			error::DebugBreak();
		}
	}

	template <Opcode SysBreakOpcode>
	void SysBreak(system_status* status, u32 instruction) {
		if constexpr (SysBreakOpcode == Opcode::SYSCALL) {
			LOG_DEBUG("CPU", "[EXCEPTION] SYSCALL Opcode at 0x{:x}",
				status->cpu->GetPc());

			u32 comment = status->cpu->GetRegs().a0;

			switch (comment)
			{
			case 0x0:
				LOG_DEBUG("CPU", "[EXCEPTION] SYSCALL::NoFunction()");
				break;
			case 0x1:
				LOG_DEBUG("CPU", "[EXCEPTION] SYSCALL::EnterCriticalSection()");
				break;
			case 0x2:
				LOG_DEBUG("CPU", "[EXCEPTION] SYSCALL::ExitCriticalSection()");
				break;
			case 0x3: {
				u32 addr = status->cpu->GetRegs().a1;
				LOG_DEBUG("CPU", "[EXCEPTION] SYSCALL::ChangeThreadSubFunction(addr=0x{:x})",
					addr);
			}
				break;
			default:
				LOG_DEBUG("CPU", "[EXCEPTION] SYSCALL::DeliverEvent(0xF0000010,0x4000)");
				break;
			}

			status->exception = true;
			status->exception_number = Excode::SYSCALL;
		}
		else if constexpr (SysBreakOpcode == Opcode::BREAK) {
			u32 comment = (instruction >> 6) & 0xFFFFF;
			LOG_ERROR("CPU", "[EXCEPTION] BREAK Opcode at 0x{:x} with arg 0x{:x}",
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
			LOG_ERROR("CPU", "[EXCEPTION] Invalid shift reg opcode {:#x} at {:#x}",
				u32(ShiftOpcode), status->cpu->GetPc());
			error::DebugBreak();
		}
	}

	template <Opcode CopLoadOpcode>
	void CoprocessorLoad(system_status* status, u32 instruction) {
		u32 rt   = (instruction >> 16) & 0x1F;
		u32 rs   = (instruction >> 21) & 0x1F;
		i32 off = i16(instruction & 0xFFFF);

		u32 base = status->cpu->GetRegs().array[rs];
		u32 address = u32(base + off);

		if constexpr (CopLoadOpcode == Opcode::LWC1) {
			LOG_ERROR("CPU", "[EXCEPTION] Invalid coprocessor {:#x} at {:#x}",
				1, status->cpu->GetPc());
			status->CoprocessorUnusableException(1);
		}
		if constexpr (CopLoadOpcode == Opcode::LWC3) {
			LOG_ERROR("CPU", "[EXCEPTION] Invalid coprocessor {:#x} at {:#x}",
				3, status->cpu->GetPc());
			status->CoprocessorUnusableException(3);
		}
		else if constexpr (CopLoadOpcode == Opcode::LWC0) {
			auto value = status->sysbus
				->Read<u32, true, true>(address);
			if (status->exception)
				return;
			status->cpu->WriteCOP0(value, u8(rt));
		}
		else if constexpr (CopLoadOpcode == Opcode::LWC2) {
			auto enable_gte = status->cpu->GetCOP0().registers.sr.cop2_enable;
			if (!enable_gte) {
				status->CoprocessorUnusableException(2);
				return;
			}

			auto value = status->sysbus
				->Read<u32, true, true>(address);
			if (status->exception)
				return;
			status->cpu->WriteCOP2_Data(value, u8(rt));
		}
		else {
			LOG_ERROR("CPU", "[EXCEPTION] Invalid cop. load opcode {:#x} at {:#x}",
				u32(CopLoadOpcode), status->cpu->GetPc());
			error::DebugBreak();
		}
	}

	template <Opcode CopStoreOpcode>
	void CoprocessorStore(system_status* status, u32 instruction) {
		u32 rt = (instruction >> 16) & 0x1F;
		u32 rs = (instruction >> 21) & 0x1F;
		i32 off = i16(instruction & 0xFFFF);

		u32 base = status->cpu->GetRegs().array[rs];
		u32 address = u32(base + off);

		if constexpr (CopStoreOpcode == Opcode::SWC1) {
			LOG_ERROR("CPU", "[EXCEPTION] Invalid coprocessor {:#x} at {:#x}",
				1, status->cpu->GetPc());
			status->CoprocessorUnusableException(1);
		}
		if constexpr (CopStoreOpcode == Opcode::LWC3) {
			LOG_ERROR("CPU", "[EXCEPTION] Invalid coprocessor {:#x} at {:#x}",
				3, status->cpu->GetPc());
			status->CoprocessorUnusableException(3);
		}
		else if constexpr (CopStoreOpcode == Opcode::SWC0) {
			auto value = status->cpu->ReadCOP0(u8(rt));
			if (status->exception)
				return;
			status->sysbus->Write<u32, true, true>(
				address, value
			);
		}
		else if constexpr (CopStoreOpcode == Opcode::SWC2) {
			auto enable_gte = status->cpu->GetCOP0().registers.sr.cop2_enable;
			if (!enable_gte) {
				status->CoprocessorUnusableException(2);
				return;
			}

			auto value = status->cpu->ReadCOP2_Data(u8(rt));
			if (status->exception)
				return;
			status->sysbus->Write<u32, true, true>(
				address, value
			);
		}
		else {
			LOG_ERROR("CPU", "[EXCEPTION] Invalid cop. store opcode {:#x} at {:#x}",
				u32(CopStoreOpcode), status->cpu->GetPc());
			error::DebugBreak();
		}
	}
}