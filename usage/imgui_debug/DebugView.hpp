#pragma once

#include <psxemu/renderer/SdlWindow.hpp>

#include <memory>

namespace psx {
	class System;
}

class DebugView {
public :
	DebugView(std::shared_ptr<psx::video::SdlWindow> win, psx::System* sys);
	~DebugView();

	using Window = psx::video::SdlWindow;

	Window* GetRawWindow() const {
		return m_win.get();
	}

	bool CloseRequest() {
		return m_win->CloseRequest();
	}

	void Update();

private :
	std::shared_ptr<Window> m_win;
	psx::System* m_psx;
	void* m_gl_ctx;
};