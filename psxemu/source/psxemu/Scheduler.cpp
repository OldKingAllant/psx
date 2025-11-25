#include <psxemu/include/psxemu/Scheduler.hpp>
#include <common/Errors.hpp>

#include <fmt/format.h>

#include <algorithm>

namespace psx {
	Scheduler::Scheduler() :
		m_curr_timestamp{}, m_last_id{},
		m_num_events{}, m_events{} {}

	u64 Scheduler::Schedule(u64 cycles, EventCallback callback, void* data) {
		if (m_num_events == MAX_EVENTS) [[unlikely]] {
			fmt::println("[SCHEDULER] Max events reached!");
			return INVALID_EVENT;
		}

		m_events[m_num_events++] = psx_event{
			.event_id = m_last_id++,
			.registered_timestamp = m_curr_timestamp,
			.trigger_timestamp = m_curr_timestamp + cycles,
			.callback = callback,
			.userdata = data
		};

		std::push_heap(m_events, m_events + m_num_events,
			[](psx_event const& l, psx_event const& r) {
				return l.trigger_timestamp > r.trigger_timestamp;
			}
		);

		return m_last_id - 1;
	}

	bool Scheduler::Deschedule(u64 event_id) {
		auto it = std::find_if(
			m_events, m_events + m_num_events,
			[event_id](psx_event const& ev) {
				return ev.event_id == event_id;
			}
		);

		if (it == m_events + m_num_events)
			return false;

		std::swap(*it, m_events[m_num_events - 1]);

		m_num_events--;

		std::make_heap(m_events, m_events + m_num_events,
			[](psx_event const& l, psx_event const& r) {
				return l.trigger_timestamp > r.trigger_timestamp;
			}
		);

		return true;
	}

	void Scheduler::Advance(u64 num_cycles, bool ignore_overflow) {
		u64 final_time = m_curr_timestamp + num_cycles;

		bool _cont = true;

		do {
			if (!m_num_events) {
				_cont = false;
				continue;
			}

			psx_event const& ev = m_events[0];

			if (ev.trigger_timestamp > final_time) {
				_cont = false;
			}
			else {
				u64 late_cycles = ignore_overflow ?
					0 : final_time - ev.trigger_timestamp;
				m_curr_timestamp = ev.trigger_timestamp + late_cycles;
				if (ev.callback)
					ev.callback(ev.userdata, late_cycles);

				std::pop_heap(m_events, m_events + m_num_events,
					[](psx_event const& l, psx_event const& r) {
						return l.trigger_timestamp > r.trigger_timestamp;
					}
				);

				m_num_events--;
			}
		} while (_cont);

		m_curr_timestamp = final_time;
	}
}