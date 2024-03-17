#include <psxemu/include/psxemu/MIPS1.hpp>
#include <psxemu/include/psxemu/SystemBus.hpp>
#include <psxemu/include/psxemu/Interpreter.hpp>

namespace psx::cpu {
	MIPS1::MIPS1(system_status* sys_status) :
		m_regs{}, m_pc{},
		m_hi{}, m_lo{},
		m_coprocessor0{}, 
		m_sys_status{sys_status} {}

	void MIPS1::StepInstruction() {
		auto bus = m_sys_status->sysbus;

		//Consider the instruction to be cached (for now)
		//and add one single clock cycle to the count
		auto instruction = bus->Read<u32, false>(m_pc);

		bus->m_curr_cycles += 1;

		cpu::InterpretMips(m_sys_status, instruction);

		if (!m_sys_status->branch) {
			m_pc += 4;
		}
		else {
			m_sys_status->branch = false;
		}
	}
}