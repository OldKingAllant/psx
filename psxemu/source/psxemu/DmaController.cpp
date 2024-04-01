#include <psxemu/include/psxemu/SystemStatus.hpp>
#include <psxemu/include/psxemu/DmaController.hpp>

#include <fmt/format.h>

namespace psx {
	DmaController::DmaController(system_status* sys_status) :
		m_sys_status{ sys_status }, m_control{}, 
		m_int_control{} {}

	void DmaController::Write(u32 address, u32 value, u32 mask) {
		if (address >= DMA_CONTROL && address < DMA_CONTROL + 4) {
			m_control.raw = value;
			fmt::println("[DMA CONTROLLER] DMA Control = 0x{:x}", m_control.raw);
			return;
		}

		if (address >= DMA_INT && address < DMA_INT + 4) {
			m_int_control.raw = value;
			fmt::println("[DMA CONTROLLER] INT Control = 0x{:x}", m_int_control.raw);
			return;
		}

		fmt::println("[DMA CONTROLLER] Accessing invalid/unused register 0x{:x}", address);
	}

	u32 DmaController::Read(u32 address) const {
		if (address >= DMA_CONTROL && address < DMA_CONTROL + 4) {
			return m_control.raw;
		}

		if (address >= DMA_INT && address < DMA_INT + 4) {
			return m_int_control.raw;
		}

		fmt::println("[DMA CONTROLLER] Accessing invalid/unused register 0x{:x}", address);
		return 0;
	}
}