#pragma once

#include <common/Defs.hpp>
#include <common/Macros.hpp>

namespace psx {
	using EventCallback = void(__cdecl *)(void*, u64);

	struct psx_event {
		u64 event_id;
		u64 registered_timestamp;
		u64 trigger_timestamp;
		EventCallback callback;
		void* userdata;
	};

	static constexpr u64 MAX_EVENTS = 60;
	static constexpr u64 INVALID_EVENT = (u64)-1;

	class Scheduler {
	public :
		Scheduler();

		FORCE_INLINE u64 GetTimestamp() const {
			return m_curr_timestamp;
		}

		FORCE_INLINE psx_event const* GetEvents() const {
			return m_events;
		}

		FORCE_INLINE u64 GetNumEvents() const {
			return m_num_events;
		}

		FORCE_INLINE u64 GetNextID() const {
			return m_last_id;
		}

		/// <summary>
		/// Schedule an event to be fired after
		/// n cycles
		/// </summary>
		/// <param name="cycles">Cycles after which the event is fired</param>
		/// <param name="callback">Event callback</param>
		/// <param name="data">Data passed to the callback</param>
		/// <returns>Event ID</returns>
		[[nodiscard]] u64 Schedule(u64 cycles, EventCallback callback, void* data);
		
		/// <summary>
		/// Remove an event
		/// </summary>
		/// <param name="event_id">ID of the event to remove</param>
		/// <returns>True if the event was in the queue</returns>
		bool Deschedule(u64 event_id);

		/// <summary>
		/// Fire all the events that happened
		/// between m_curr_timestamp 
		/// and m_curr_timestamp + num_cycles
		/// </summary>
		/// <param name="num_cycles"></param>
		void Advance(u64 num_cycles);

	private :
		u64 m_curr_timestamp;
		u64 m_last_id;
		u64 m_num_events;
		psx_event m_events[MAX_EVENTS];
	};
}