#include <psxemu/include/psxemu/System.hpp>

namespace psx {
	System::System() :
		m_cpu{}, m_sysbus{&m_status},
		m_status{}
	{
		m_status.cpu = &m_cpu;
		m_status.sysbus = &m_sysbus;
	}
}