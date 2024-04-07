#pragma once

#include "Recompiler.hpp"
#include "CodeCache.hpp"
#include "cop0.hpp"

#include <functional>

namespace psx {
	struct system_status;
}

namespace psx::cpu {
	union Registers {
#pragma pack(push, 4)
		struct {
			u32 zero;
			u32 at;
			u32 v0;
			u32 v1;
			u32 a0;
			u32 a1;
			u32 a2;
			u32 a3;
			u32 t0;
			u32 t1;
			u32 t2;
			u32 t3;
			u32 t4;
			u32 t5;
			u32 t6;
			u32 t7;
			u32 s0;
			u32 s1;
			u32 s2;
			u32 s3;
			u32 s4;
			u32 s5;
			u32 s6;
			u32 s7;
			u32 t8;
			u32 t9;
			u32 k0;
			u32 k1;
			u32 gp;
			u32 sp;
			u32 fp;
			u32 ra;
		};
#pragma pack(pop)

		u32 array[33];
	};

	static constexpr u8 InvalidReg = 32;

	class MIPS1 {
	public :
		MIPS1(system_status* sys_status);

		FORCE_INLINE Registers& GetRegs() { return m_regs; }
		FORCE_INLINE u32& GetPc() { return m_pc; }
		FORCE_INLINE u32& GetHI() { return m_hi; }
		FORCE_INLINE u32& GetLO() { return m_lo; }
		FORCE_INLINE cop0& GetCOP0() { return m_coprocessor0; }

		void StepInstruction();

		void WriteCOP0(u32 value, u8 cop0_reg);
		void ReadCOP0(u8 cop0_reg, u8 dest_reg);

		void FlushLoadDelay();

		bool HLE_Bios(u32 address);

		void SetHLEHandler(std::function<bool(u32)>&& handler) {
			m_hle_bios_handler = handler;
		}

		void InterlockHiLo();

		void UpdateLoadDelay();
		void UpdateRegWriteback();

	private:
		bool CheckInterrupts();
		bool CheckInstructionGTE();

	private :
		Registers m_regs;
		u32 m_pc;
		u32 m_hi;
		u32 m_lo;

		cop0 m_coprocessor0;

		system_status* m_sys_status;

		std::function<bool(u32)> m_hle_bios_handler;
	};
}