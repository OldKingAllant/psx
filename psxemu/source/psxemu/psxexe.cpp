#include <psxemu/include/psxemu/psxexe.hpp>

namespace psx {
	psxexe::psxexe(u8* data) : m_data{data} {}

	psxexe::~psxexe() {
		if (m_data)
			delete[] m_data;
	}
}