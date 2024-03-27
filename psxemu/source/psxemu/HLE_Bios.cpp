#include <psxemu/include/psxemu/System.hpp>

#include <iostream>

namespace psx {
	void System::HLE_Putchar() {
		u8 the_char = m_cpu.GetRegs().array[4];

		if (m_putchar)
			m_putchar((char)the_char);
	}

	void System::HLE_Getchar() {

	}
}