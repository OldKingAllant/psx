#include <psxemu/include/psxemu/SystemStatus.hpp>
#include <psxemu/include/psxemu/RootCounters.hpp>

#include <common/Errors.hpp>

#include <fmt/format.h>

namespace psx {
	RootCounter::RootCounter(u32 id, system_status* sys_status) :
		m_sys_status{ sys_status }, m_counter_id{id}, 
		m_count_value{}, m_count_target{}, m_oneshot_flag{false},
		m_count_partial{}, m_dotclock_sel{}, m_cycles_per_inc{},
		m_target_event_id{}, m_ov_event_id{}, m_mode{} {}

	void RootCounter::SetDotclock(u16 sel) {
		m_dotclock_sel = sel;

		if (m_counter_id != 0)
			return;

		fmt::println("[COUNTER0] Dotclock changed {}", sel);

		ClockSource0 clk_src = static_cast<ClockSource0>(m_mode.clock_src);

		if (clk_src != ClockSource0::DOTCLOCK1 &&
			clk_src != ClockSource0::DOTCLOCK2)
			return;

		m_cycles_per_inc = (u16)DOT_CYCLES[m_dotclock_sel];
		m_count_partial = 0;

		UpdateEvents();
	}

	u16 RootCounter::ComputeCyclesPerInc() const {
		switch (m_counter_id)
		{
		case 0: {
			ClockSource0 clk_src = static_cast<ClockSource0>(m_mode.clock_src);

			switch (clk_src)
			{
			case psx::ClockSource0::SYS_CLOCK1:
			case psx::ClockSource0::SYS_CLOCK2:
				return 1;
			case psx::ClockSource0::DOTCLOCK1:
			case psx::ClockSource0::DOTCLOCK2:
				return (u16)DOT_CYCLES[m_dotclock_sel];
			default:
				error::Unreachable();
				break;
			}
		}
		break;
		case 1: {
			ClockSource1 clk_src = static_cast<ClockSource1>(m_mode.clock_src);

			switch (clk_src)
			{
			case psx::ClockSource1::SYS_CLOCK1:
			case psx::ClockSource1::SYS_CLOCK2:
				return 1;
				break;
			case psx::ClockSource1::HBLANK1:
			case psx::ClockSource1::HBLANK2:
				return CLOCKS_SCANLINE;
				break;
			default:
				error::Unreachable();
				break;
			}
		}
		break;
		case 2: {
			ClockSource2 clk_src = static_cast<ClockSource2>(m_mode.clock_src);

			switch (clk_src)
			{
			case psx::ClockSource2::SYS_CLOCK1:
			case psx::ClockSource2::SYS_CLOCK2:
				return 1;
				break;
			case psx::ClockSource2::SYS_CLOCK81:
			case psx::ClockSource2::SYS_CLOCK82:
				return 8;
				break;
			default:
				error::Unreachable();
				break;
			}
		}
		break;
		default:
			error::Unreachable();
			break;
		}

		return 0;
	}

	void RootCounter::Write(u32 address, u32 value) {
		//
	}

	u32 RootCounter::Read(u32 address) {
		return 0;
	}

	void RootCounter::HBlank() {
		//
	}

	void RootCounter::VBlank() {
		//
	}

	void RootCounter::UpdateCounter(u32 cycles) {
		//
	}

	void RootCounter::UpdateEvents() {
		//
	}

	void RootCounter::FireIRQ(IrqSource source) {
		//
	}
}