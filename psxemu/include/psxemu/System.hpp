#pragma once

#include "MIPS1.hpp"

namespace psx {
	class System {
	public :
		FORCE_INLINE cpu::MIPS1& GetCPU() { return m_cpu; }

	private :
		cpu::MIPS1 m_cpu;
	};
}