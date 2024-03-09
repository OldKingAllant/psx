#pragma once

#include "Recompiler.hpp"
#include "CodeCache.hpp"
#include "cop0.hpp"

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

		u32 array[32];
	};

	class MIPS1 {
	public :
		FORCE_INLINE Registers& GetRegs() { return m_regs; }
		FORCE_INLINE u32& GetPc() { return m_pc; }
		FORCE_INLINE u32& GetHI() { return m_hi; }
		FORCE_INLINE u32& GetLO() { return m_lo; }
		FORCE_INLINE cop0& GetCOP0() { return m_coprocessor0; }

	private :
		Registers m_regs;
		u32 m_pc;
		u32 m_hi;
		u32 m_lo;

		cop0 m_coprocessor0;
	};
}