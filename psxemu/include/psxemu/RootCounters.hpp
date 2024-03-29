#pragma once

#include <common/Defs.hpp>
#include <common/Macros.hpp>

//System clock
//Dotclock
//Hblank

namespace psx {
	struct system_status;

	enum class SyncMode0 : u8 {
		PAUSE,
		RESET,
		RESET_AND_PAUSE,
		PAUSE_AND_SWITCH
	};

	enum class SyncMode2 : u8 {
		STOP1,
		FREE_RUN1,
		STOP2,
		FREE_RUN2
	};

	enum class ClockSource0 : u8 {
		SYS_CLOCK1,
		DOTCLOCK1,
		SYS_CLOCK2,
		DOTCLOCK2
	};

	enum class ClockSource1 : u8 {
		SYS_CLOCK1,
		HBLANK1,
		SYS_CLOCK2,
		HBLANK2,
	};

	enum class ClockSource2 : u8 {
		SYS_CLOCK1,
		SYS_CLOCK81,
		SYS_CLOCK2,
		SYS_CLOCK82,
	};

	enum class IrqSource {
		OV,
		TARGET
	};
	
	class RootCounter {
	public :
		RootCounter(u32 id, system_status* sys_status);

		/// <summary>
		/// Selects the source dotclock
		/// </summary>
		/// <param name="sel">Index of new dotclock</param>
		void SetDotclock(u16 sel);

		/// <summary>
		/// Write a single hardware register
		/// (not that 8/16 bits writes behave like
		/// 32 bit writes)
		/// </summary>
		/// <param name="address">Address relative to counter base</param>
		/// <param name="value">Value to write</param>
		void Write(u32 address, u32 value);

		/// <summary>
		/// Read an hardware register
		/// </summary>
		/// <param name="address">Address relative to counter base</param>
		/// <returns>Value of register</returns>
		u32 Read(u32 address);

		/// <summary>
		/// Signal an HBLANK (for counter 0)
		/// </summary>
		void HBlank();

		/// <summary>
		/// Signal a VBLANK (counter 1)
		/// </summary>
		void VBlank();

	private :
		/// <summary>
		/// Since we are updating the counter
		/// only when an event happens, we want
		/// to compute the counter value on the
		/// fly when someone reads the value
		/// register
		/// </summary>
		/// <param name="cycles">Advance by this number of cycles</param>
		void UpdateCounter(u32 cycles);

		/// <summary>
		/// Precompute the number of cycles required
		/// to increment the counter value a single time
		/// </summary>
		/// <returns></returns>
		u16 ComputeCyclesPerInc() const;

		/// <summary>
		/// Deschedule/Reschedule new events based
		/// on configuration/counter/target value
		/// </summary>
		void UpdateEvents();

		/// <summary>
		/// Check if an IRQ should be fired or not 
		/// depending on its source (and config)
		/// </summary>
		/// <param name="source">IRQ source</param>
		void FireIRQ(IrqSource source);

	private :
		system_status* m_sys_status;
		u32 m_counter_id;
		u16 m_count_value;
		u16 m_count_target;
		bool m_oneshot_flag;
		u16 m_count_partial;
		u16 m_dotclock_sel;
		u16 m_cycles_per_inc;

		u64 m_target_event_id;
		u64 m_ov_event_id;

		union {
			struct {
				bool m_sync_enable : 1;
				u8 sync_mode : 2;
				bool reset_on_target : 1;
				bool irq_on_target : 1;
				bool irq_on_ov : 1;
				bool irq_repeat : 1;
				bool irq_toggle : 1;
				u8 clock_src : 2;
				bool irq : 1;
				bool target_reached : 1;
				bool ov_reached : 1;
			};

			u16 raw;
		} m_mode;
	};
}