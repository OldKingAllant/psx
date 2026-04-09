#pragma once

#include <psxemu/renderer/SdlWindow.hpp>
#include <psxemu/renderer/GLContext.hpp>
#include <psxemu/include/psxemu/MCFilesystem.hpp>
#include <psxemu/include/psxemu/formats/GenericSaveFile.hpp>

#include <memory>
#include <unordered_map>
#include <optional>
#include <array>

namespace psx {
	class System;
	struct DriveCommand;
	struct GPUCommand;
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

	struct HighlitArea {
		std::string name;
		uint32_t num_vertices{};
		int32_t x[4];
		int32_t y[4];
		bool filled = true;
		size_t cmd_index;
		std::optional<std::array<float, 4>> color;
	};

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
	
	void KernelWindow();
	void DriveWindow();
	void MdecWindow();
	void MemcardWindow(psx::kernel::MCFs& mcfs, psx::u32 slot);

	void GpuCommandWindow();
	void GpuVramWindow();
	void GpuDumpVramWindow();
	void GpuLoadDumpWindow();
	void GpuMainWindow();
	void GpuWindow();

	void TaskBarWindow();

	void ShowTimerImpl(uint32_t tmr_id);

	void ShowDriveCommand(psx::DriveCommand const* cmd);

	bool ShowGpuCommandEntry(size_t index, psx::GPUCommand const* cmd, bool has_details);
	void ShowGpuCommandDetails(psx::GPUCommand const* cmd, size_t cmd_index, bool is_popup);
	void ShowGpuCmdPolygon(psx::GPUCommand const* cmd, size_t cmd_index);
	void ShowGpuCmdRectangle(psx::GPUCommand const* cmd, size_t cmd_index);
	void ShowGpuCmdLine(psx::GPUCommand const* cmd, size_t cmd_index);
	void ShowGpuCmdData(psx::GPUCommand const* cmd, size_t cmd_index);
	void ShowGpuCmdTexture(psx::GPUCommand const* cmd, size_t cmd_index);

	std::string GetGpuCommandName(psx::GPUCommand const* cmd);
	bool GetGpuCommandHasDetails(psx::GPUCommand const* cmd);
	void GpuCommandAppendVramAreas(psx::GPUCommand const* cmd, size_t cmd_index);
	void GpuCommandAppendPossiblyOverflowedAreas(uint32_t x, uint32_t y,
		uint32_t w, uint32_t h, std::string name, size_t cmd_index,
		std::optional<std::array<float, 4>> color = std::nullopt);
	void GpuCommandLoadConfig(psx::GPUCommand const* cmd);
	void GpuCommandAppendClipRect(psx::GPUCommand const* cmd);

	std::optional<psx::u32> CreateTextureFromIcon(std::vector<psx::u32> icon_data);
	void FreeTexture(psx::u32 handle);

private :
	std::shared_ptr<Window> m_win;
	psx::System* m_psx;
	psx::video::GLContext* m_gl_ctx;

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

	std::list<HighlitArea> m_highlited_areas;
	
	struct GPUSavedConf {
		uint32_t x_top, y_top;
		uint32_t x_bot, y_bot;
		int32_t x_off, y_off;
	} m_gpu_saved_conf;

	std::unique_ptr<psx::video::Shader> m_texture_view_shader;

	std::unordered_map<std::string, bool> m_is_main_window_open;
};