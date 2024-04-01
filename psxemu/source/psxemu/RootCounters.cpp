#include <psxemu/include/psxemu/SystemStatus.hpp>
#include <psxemu/include/psxemu/RootCounters.hpp>
#include <psxemu/include/psxemu/Interrupts.hpp>

#include <common/Errors.hpp>

#include <fmt/format.h>

namespace psx {
	RootCounter::RootCounter(u32 id, system_status* sys_status) :
		m_sys_status{ sys_status }, m_counter_id{id}, 
		m_count_value{}, m_count_target{}, m_oneshot_flag{false},
		m_count_partial{}, m_dotclock_sel{}, m_cycles_per_inc{},
		m_curr_event_id{}, m_mode{},
		m_last_update_timestamp{}, m_stopped{false}, 
		m_vblank{ false }, m_hblank{false} {
		m_curr_event_id = INVALID_EVENT;
	}

	void RootCounter::SetDotclock(u16 sel) {
		//This method must be called by the GPU 
		//when the horizontal resolutiona changes
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

		//Interrupts might happen sooner/later
		//due to different clock speed
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

	static constexpr u32 VALUE_ADDRESS = 0x0;
	static constexpr u32 MODE_ADDRESS = 0x4;
	static constexpr u32 TARGET_ADDRESS = 0x8;

	void RootCounter::Write(u32 address, u32 value) {
		if (address >= VALUE_ADDRESS
			&& address < VALUE_ADDRESS + 4) {
			//Writing a value greater than the target
			//does not trigger IRQ/reset
			fmt::println("[COUNTER{}] Value set to 0x{:x}", 
				m_counter_id, (u16)value);
			m_count_value = (u16)value;
			//Events might happen sooner/later
			UpdateEvents();
			return;
		}

		if (address >= MODE_ADDRESS
			&& address < MODE_ADDRESS + 4) {
			//Resets current counter value to 0
			m_count_value = 0;
			m_count_partial = 0;
			//Resets the oneshot condition (IRQ can happen again)
			m_oneshot_flag = false;

			bool target_reached = m_mode.target_reached;
			bool ov_reached = m_mode.ov_reached;

			m_mode.raw = (u16)value;

			//Keep these values (no set when writing)
			m_mode.target_reached = target_reached;
			m_mode.ov_reached = ov_reached;

			//Set to 1 on write (0 = irq, 1 = no irq)
			m_mode.irq = true;

			//Remove/Insert new events if necessary
			m_cycles_per_inc = ComputeCyclesPerInc();
			UpdateEvents();
			fmt::println("[COUNTER{}] Mode set to 0x{:x}", 
				m_counter_id, (u16)value);
			return;
		}

		if (address >= TARGET_ADDRESS &&
			address < TARGET_ADDRESS + 4) {
			m_count_target = (u16)value;
			fmt::println("[COUNTER{}] Target set to 0x{:x}",
				m_counter_id, (u16)value);
			UpdateEvents();
			return;
		}

		fmt::println("[COUNTER{}] Accessing invalid register 0x{:x}",
			m_counter_id, address);
	}

	u32 RootCounter::Read(u32 address) {
		if (address >= VALUE_ADDRESS
			&& address < VALUE_ADDRESS + 4) {
			//We want to update the counter value
			//to sync it
			u64 curr_time = m_sys_status->scheduler.GetTimestamp();
			u64 diff = curr_time - m_last_update_timestamp;
			UpdateCounter(diff);
			return m_count_value;
		}

		if (address >= MODE_ADDRESS
			&& address < MODE_ADDRESS + 4) {
			u32 curr_mode = m_mode.raw;

			//Reset on read
			m_mode.target_reached = false;
			m_mode.ov_reached = false;

			return curr_mode;
		}

		if (address >= TARGET_ADDRESS &&
			address < TARGET_ADDRESS + 4) {
			//Nothing happens here
			return m_count_target;
		}

		fmt::println("[COUNTER{}] Accessing invalid register 0x{:x}",
			m_counter_id, address);
		return 0;
	}

	/*
	For all HBLANK/VBLANK events,
	timer 0 considers the first type
	and timer 1 the second.
	Since this events can reset/stop/start
	the counter, we need first to update
	the counter value to an approximated value
	(to adjust event scheduling) and then
	deschedule the now old (and probably invalid)
	events with new ones
	*/

	void RootCounter::HBlank() {
		m_hblank = true;

		u64 curr_time = m_sys_status->scheduler.GetTimestamp();
		u64 diff = curr_time - m_last_update_timestamp;
		UpdateCounter(diff);

		if (m_counter_id != 0 || !m_mode.m_sync_enable)
			return;

		auto sync_mode = static_cast<SyncMode0>(m_mode.sync_mode);

		switch (sync_mode)
		{
		case psx::SyncMode0::PAUSE: {
			m_stopped = true;
		}
		break;
		case psx::SyncMode0::RESET: {
			m_count_value = 0;
		}
		break;
		case psx::SyncMode0::RESET_AND_PAUSE: {
			m_stopped = false;
			m_count_value = 0;
		}
		break;
		case psx::SyncMode0::PAUSE_AND_SWITCH: {
			m_stopped = false;
			m_mode.m_sync_enable = false;
		}
		break;
		default:
			error::Unreachable();
			break;
		}

		UpdateEvents();
	}

	void RootCounter::VBlank() {
		m_vblank = true;

		u64 curr_time = m_sys_status->scheduler.GetTimestamp();
		u64 diff = curr_time - m_last_update_timestamp;
		UpdateCounter(diff);

		if (m_counter_id != 1 || !m_mode.m_sync_enable)
			return;

		auto sync_mode = static_cast<SyncMode1>(m_mode.sync_mode);

		switch (sync_mode)
		{
		case psx::SyncMode1::PAUSE: {
			m_stopped = true;
		}
		break;
		case psx::SyncMode1::RESET: {
			m_count_value = 0;
		}
		break;
		case psx::SyncMode1::RESET_AND_PAUSE: {
			m_stopped = false;
			m_count_value = 0;
		}
		break;
		case psx::SyncMode1::PAUSE_AND_SWITCH: {
			m_stopped = false;
			m_mode.m_sync_enable = false;
		}
		break;
		default:
			error::Unreachable();
			break;
		}

		UpdateEvents();
	}

	void RootCounter::HBlankEnd() {
		m_hblank = false;

		u64 curr_time = m_sys_status->scheduler.GetTimestamp();
		u64 diff = curr_time - m_last_update_timestamp;
		UpdateCounter(diff);

		if (m_counter_id != 0 || !m_mode.m_sync_enable)
			return;

		auto sync_mode = static_cast<SyncMode0>(m_mode.sync_mode);

		switch (sync_mode)
		{
		case psx::SyncMode0::PAUSE: {
			m_stopped = false;
		}
		break;
		case psx::SyncMode0::RESET: {}
		break;
		case psx::SyncMode0::RESET_AND_PAUSE: {
			m_stopped = true;
		}
		break;
		case psx::SyncMode0::PAUSE_AND_SWITCH: {}
		break;
		default:
			error::Unreachable();
			break;
		}

		UpdateEvents();
	}

	void RootCounter::VBlankEnd() {
		m_vblank = false;

		u64 curr_time = m_sys_status->scheduler.GetTimestamp();
		u64 diff = curr_time - m_last_update_timestamp;
		UpdateCounter(diff);

		if (m_counter_id != 1 || !m_mode.m_sync_enable)
			return;

		auto sync_mode = static_cast<SyncMode1>(m_mode.sync_mode);

		switch (sync_mode)
		{
		case psx::SyncMode1::PAUSE: {
			m_stopped = false;
		}
		break;
		case psx::SyncMode1::RESET: {}
		break;
		case psx::SyncMode1::RESET_AND_PAUSE: {
			m_stopped = true;
		}
		break;
		case psx::SyncMode1::PAUSE_AND_SWITCH: {}
		break;
		default:
			error::Unreachable();
			break;
		}

		UpdateEvents();
	}

	void RootCounter::UpdateCounter(u64 cycles) {
		if (m_stopped)
			return;

		auto old_count_value = m_count_value;

		m_count_partial += (u16)cycles;
		m_count_value += m_count_partial / m_cycles_per_inc;
		m_count_partial %= m_cycles_per_inc;

		m_last_update_timestamp = m_sys_status->scheduler.GetTimestamp();

		//Avoid akward cases where the application
		//can read a value greater than the target value
		if(m_mode.reset_on_target)
			m_count_value = std::min(m_count_target, m_count_value);
	}

	void overflow_callback(void* counter_ptr, u64 cycles_late);
	void target_callback(void* counter_ptr, u64 cycles_late);

	void overflow_callback(void* counter_ptr, u64 cycles_late) {
		RootCounter* counter = std::bit_cast<RootCounter*>(counter_ptr);

		counter->FireIRQ(IrqSource::OV);

		counter->m_count_value = 0;

		u64 till_ov = counter->CyclesTillOverflow() - cycles_late;
		bool neg = false;
		u64 till_target = counter->CyclesTillTarget(neg) - cycles_late;

		if (neg || till_ov <= till_target) {
			if ((u64)counter->m_count_target * counter->m_cycles_per_inc < cycles_late) {
				counter->FireIRQ(IrqSource::TARGET);
			}

			counter->m_curr_event_id = counter->m_sys_status->scheduler.Schedule(till_ov, overflow_callback, counter_ptr);
		}
		else {
			counter->m_curr_event_id = counter->m_sys_status->scheduler.Schedule(till_target, target_callback, counter_ptr);
		}
	}

	void target_callback(void* counter_ptr, u64 cycles_late) {
		RootCounter* counter = std::bit_cast<RootCounter*>(counter_ptr);

		counter->FireIRQ(IrqSource::TARGET);

		//For eventual problems, look at the overflow callback

		if (counter->m_mode.reset_on_target)
			counter->m_count_value = 0;
		else
			counter->m_count_value = counter->m_count_target + 1; //Do not reset

		u64 till_ov = counter->CyclesTillOverflow() - cycles_late;
		bool neg = false;
		u64 till_target = counter->CyclesTillTarget(neg) - cycles_late;

		if (neg || till_ov <= till_target)
			counter->m_curr_event_id = counter->m_sys_status->scheduler.Schedule(till_ov, overflow_callback, counter_ptr);
		else
			counter->m_curr_event_id = counter->m_sys_status->scheduler.Schedule(till_target, target_callback, counter_ptr);
	}

	void RootCounter::UpdateEvents() {
		//First, remove the event (even if it might still be valid)
		m_sys_status->scheduler.Deschedule(m_curr_event_id);

		if (m_mode.m_sync_enable) {
			//Enforce all sync modes for all the counters
			if (m_counter_id == 2 && (m_mode.sync_mode == 0
				|| m_mode.sync_mode == 3)) {
				m_stopped = true;
				return;
			}
			else if (m_counter_id == 0) {
				if (m_hblank && (m_mode.sync_mode == 0)) {
					m_stopped = true;
					return;
				}
				else if (!m_hblank && (m_mode.sync_mode == 2)) {
					m_stopped = true;
					return;
				}
			}
			else if (m_counter_id == 1) {
				if (m_vblank && (m_mode.sync_mode == 0)) {
					m_stopped = true;
					return;
				}
				else if (!m_vblank && (m_mode.sync_mode == 2)) {
					m_stopped = true;
					return;
				}
			}
		}

		//Be sure that this variable is false
		m_stopped = false;

		//Register a single event (the nearest one)
		//We could register both the ov and the target,
		//but that should not be necessary

		u64 till_ov = CyclesTillOverflow();

		bool neg = false;

		u64 till_target = CyclesTillTarget(neg);

		if (m_count_target == m_count_value || m_count_target == 0) {
			m_mode.target_reached = true;
			m_curr_event_id = m_sys_status->scheduler.Schedule(till_ov, overflow_callback, this);
		}
		else {
			if (neg || till_ov <= till_target)
				m_curr_event_id = m_sys_status->scheduler.Schedule(till_ov, overflow_callback, this);
			else
				m_curr_event_id = m_sys_status->scheduler.Schedule(till_target, target_callback, this);
		}
	}

	void RootCounter::FireIRQ(IrqSource source) {
		//All events should have been descheduled
		//in case of a stopped counter... Still,
		//we will not panic
		if (m_stopped) {
			fmt::println("[COUNTER{}] IRQ Trigger even if stopped!",
				m_counter_id);
			return; 
		}


		//In theory (not sure though)
		//the target and overflow flags
		//are set even if their interrupts
		//are disabled
		if (source == IrqSource::TARGET) {
			m_mode.target_reached = true;

			if (!m_mode.irq_on_target)
				return;
		}
		else if (source == IrqSource::OV) {
			m_mode.ov_reached = true;

			if (!m_mode.irq_on_ov)
				return;
		}

		if (!m_mode.irq_repeat) {
			//Oneshot mode has already been triggered,
			//Wait for reset
			if (m_oneshot_flag)
				return;

			//Condition is now triggered
			m_oneshot_flag = true;
		}

		if (!m_mode.irq_toggle) {
			if(!m_mode.irq) //Not sure IF the application should reset this value 
							//or if I should do it myself
				fmt::println("[COUNTER{}] IRQ Bit not reset!",
					m_counter_id);

			m_mode.irq = false;
		}
		else {
			m_mode.irq = !m_mode.irq;
		}

		//This conditions makes sure that
		//the interrupt is requested only
		//if the irq flag is zero, so that
		//the irq toggle mode works
		if (!m_mode.irq)
			m_sys_status->interrupt_request |= (u32)(Interrupts::TIMER0) << m_counter_id;
	}

	u64 RootCounter::CyclesTillIRQ() const {
		u64 ov_diff = 0x10000 - (u64)m_count_value;
		int target_diff = m_count_target - m_count_value;

		if (target_diff < 0)
			return ov_diff * m_cycles_per_inc;
		else
			return std::min((u64)target_diff, ov_diff) * m_cycles_per_inc;
	}

	u64 RootCounter::CyclesTillOverflow() const {
		u64 ov_diff = 0x10000 - (u64)m_count_value;
		return ov_diff * m_cycles_per_inc;
	}

	u64 RootCounter::CyclesTillTarget(bool& neg) const {
		int target_diff = m_count_target - m_count_value;

		if (target_diff < 0) {
			neg = true;
			return 0;
		}

		return (u64)target_diff * m_cycles_per_inc;
	}
}