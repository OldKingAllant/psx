#pragma once

#include <map>
#include <set>

#include "SdlWindow.hpp"

union SDL_Event;

namespace psx::video {
	struct WindowManagerOpts {
		/// <summary>
		/// If a window handles an
		/// event, propagate it
		/// also to other windows
		/// (works only if ignore_id
		/// is true)
		/// </summary>
		bool propagate_event;

		/// <summary>
		/// Deliver an event to all
		/// windows, stop event propagation
		/// if a window handles the event
		/// and propagate_event is false
		/// </summary>
		bool ignore_id;

		/// <summary>
		/// Ignore these event ids
		/// </summary>
		std::set<uint32_t> ignored_events;

		/// <summary>
		/// If an event is ignored, 
		/// use SDL_PushEvent to
		/// reinsert it in the queue
		/// </summary>
		bool queue_ignored_events;

		/// <summary>
		/// Create default configuration
		/// 1. Do not propagate events
		/// 2. Do not ignore ids
		/// 3. Do not ignore any event
		/// 4. Do not queue ignored events
		/// </summary>
		WindowManagerOpts() :
			propagate_event{false},
			ignore_id{false},
			ignored_events{},
			queue_ignored_events{false} {}
	};

	

	/// <summary>
	/// This class is simply a wrapper which
	/// purpose is delivering SDL events to
	/// the correct window. This might be necessary
	/// since SDL has no concept of event ownership,
	/// and events are global.
	/// This class does NOT have ownership of
	/// the windows
	/// </summary>
	class WindowManager {
	public :
		/// <summary>
		/// Create with default config
		/// </summary>
		WindowManager();

		WindowManager(WindowManager const&) = delete;

		~WindowManager();

		/// <summary>
		/// Set new config (old options
		/// NOT preserved)
		/// </summary>
		/// <param name="options">New options</param>
		void SetConfig(WindowManagerOpts const& options);

		void SetPropagate(bool propagate);
		void SetIgnore(bool ignore);
		void SetIgnoredEvent(SdlEvent ev);
		void SetQueueIgnored(bool queue);

		/// <summary>
		/// Deliver event following config
		/// </summary>
		/// <param name="event">Event to deliver</param>
		void DeliverEvent(SDL_Event* event);

		/// <summary>
		/// Deliver event to ALL windows irrespective
		/// of configuration
		/// </summary>
		/// <param name="event">Event to deliver</param>
		void DeliverForceIgnoreID(SDL_Event* event);

		/// <summary>
		/// Use SDL_PollEvent
		/// and deliver
		/// </summary>
		/// <returns>If SDL_QUIT has been received</returns>
		bool HandleEvents();

		/// <summary>
		/// Add new window to receive events
		/// </summary>
		/// <param name="window">Pointer to window</param>
		void AddWindow(SdlWindow* window);

		/// <summary>
		/// Opposite of AddWindow
		/// </summary>
		/// <param name="window">Pointer to window</param>
		void RemoveWindow(SdlWindow* window);

	private :
		uint32_t SdlEventToEventID(SdlEvent ev) const;

		bool FilterEvent(SDL_Event* ev);

	private :
		WindowManagerOpts m_opts{};
		std::map<uint32_t, SdlWindow*> m_windows{};
	};
}