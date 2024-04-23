#include "WindowManager.hpp"

#include <SDL2/SDL.h>

#include <stdexcept>

namespace psx::video {
	WindowManager::WindowManager() :
		m_opts{}, m_windows{} {}

	WindowManager::~WindowManager() {}

	void WindowManager::SetConfig(WindowManagerOpts const& options) {
		m_opts = options;
	}

	void WindowManager::SetPropagate(bool propagate) {
		m_opts.propagate_event = propagate;
	}

	void WindowManager::SetIgnore(bool ignore) {
		m_opts.ignore_id = ignore;
	}

	void WindowManager::SetIgnoredEvent(SdlEvent ev) {
		m_opts.ignored_events.insert(SdlEventToEventID(ev));
	}

	void WindowManager::SetQueueIgnored(bool queue) {
		m_opts.queue_ignored_events = queue;
	}

	
	void WindowManager::DeliverEvent(SDL_Event* event) {
		if (event->type != SDL_WINDOWEVENT) {
			DeliverForceIgnoreID(event);
			return;
		}

		auto window_id = event->window.windowID;
		auto win_pointer = m_windows.find(window_id);

		if (win_pointer == m_windows.end())
			return;

		win_pointer->second->HandleEvent(event);
	}

	
	void WindowManager::DeliverForceIgnoreID(SDL_Event* event) {
		for (auto const& [id, window] : m_windows) {
			if (window->HandleEvent(event) && !m_opts.propagate_event)
				break;
		}
	}

	bool WindowManager::FilterEvent(SDL_Event* ev) {
		if (m_opts.ignored_events.contains(ev->type))
			return false;

		if (ev->type == SDL_WINDOWEVENT) {
			if (m_opts.ignored_events.contains(ev->window.event))
				return false;
		}

		return true;
	}

	
	bool WindowManager::HandleEvents() {
		SDL_Event ev{};

		std::vector<SDL_Event> ignored_events;

		while (SDL_PollEvent(&ev)) {
			if (ev.type == SDL_QUIT)
				return false;

			for (SdlWindow* win : m_unfiltered_windows)
				win->HandleEvent(&ev);

			if (FilterEvent(&ev)) {
				if (m_opts.ignore_id)
					DeliverForceIgnoreID(&ev);
				else
					DeliverEvent(&ev);
			}
			else if (m_opts.queue_ignored_events) {
				ignored_events.push_back(ev);
			}
		}

		for (auto& ev : ignored_events)
			SDL_PushEvent(&ev);

		return true;
	}

	
	void WindowManager::AddWindow(SdlWindow* window) {
		uint32_t id = window->GetWindowID();

		if (m_windows.contains(id))
			return;

		m_windows.insert(std::pair{ id, window });
	}

	
	void WindowManager::RemoveWindow(SdlWindow* window) {
		m_windows.erase(window->GetWindowID());
	}

	uint32_t WindowManager::SdlEventToEventID(SdlEvent ev) const {
		switch (ev)
		{
		case psx::video::SdlEvent::KeyPressed:
			return SDL_KEYDOWN;
			break;
		case psx::video::SdlEvent::KeyReleased:
			return SDL_KEYUP;
			break;
		case psx::video::SdlEvent::WindowClose:
			return SDL_WINDOWEVENT_CLOSE;
			break;
		case psx::video::SdlEvent::WindowResized:
			return SDL_WINDOWEVENT_RESIZED;
			break;
		case psx::video::SdlEvent::WindowSizeChanged:
			return SDL_WINDOWEVENT_SIZE_CHANGED;
			break;
		case psx::video::SdlEvent::Quit:
			return SDL_QUIT;
			break;
		default:
			throw std::runtime_error("Invalid event type!");
			break;
		}
	}

	void WindowManager::SetWindowAsUnfiltered(SdlWindow* window) {
		uint32_t id = window->GetWindowID();

		if (!m_windows.contains(id))
			return;

		m_unfiltered_windows.insert(window);
	}
}