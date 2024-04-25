#pragma once

#include <psxemu/renderer/SdlWindow.hpp>

#include <memory>
#include <map>

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
	/// <summary>
	/// Draw the CPU status window
	/// Since CPU registers can be
	/// more easily managed by 
	/// using the GDB stub, this
	/// view is only useful for
	/// COP0 registers
	/// </summary>
	void CpuWindow();
	void DmaWindow();
	void MemoryConfigWindow();

private :
	std::shared_ptr<Window> m_win;
	psx::System* m_psx;
	void* m_gl_ctx;

	bool m_first_frame;
	std::map<std::string, bool> m_enabled_opts;
};