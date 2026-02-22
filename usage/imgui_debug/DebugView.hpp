#pragma once

#include <psxemu/renderer/SdlWindow.hpp>
#include <psxemu/include/psxemu/MCFilesystem.hpp>
#include <psxemu/include/psxemu/formats/GenericSaveFile.hpp>

#include <memory>
#include <unordered_map>
#include <optional>

namespace psx {
	class System;
	struct DriveCommand;
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
	void TimersWindow();
	void GpuWindow();
	void KernelWindow();
	void DriveWindow();
	void MdecWindow();
	void MemcardWindow(psx::kernel::MCFs& mcfs, psx::u32 slot);

	void ShowTimerImpl(uint32_t tmr_id);

	void ShowDriveCommand(psx::DriveCommand const* cmd);

	std::optional<psx::u32> CreateTextureFromIcon(std::vector<psx::u32> icon_data);
	void FreeTexture(psx::u32 handle);

private :
	std::shared_ptr<Window> m_win;
	psx::System* m_psx;
	void* m_gl_ctx;

	bool m_first_frame;
	std::unordered_map<std::string, bool> m_enabled_opts;

	/// <summary>
	/// Flag is set to true when kernel's
	/// exception chains are init 
	/// (using an exit hook on 
	/// InstallExceptionHandlers)
	/// </summary>
	bool m_except_init;
	uint64_t m_except_init_hook;

	std::unordered_map<std::string, std::pair<psx::kernel::GenericSaveFile, psx::u32>>  m_tracked_mc_files[2];
};