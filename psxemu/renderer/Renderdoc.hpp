#pragma once

#include <string>
#include <memory>

namespace psx::video {
	struct ApiPointers;
	struct ModuleHandle;

	enum class RenderDocOverlayOptions {
		DISABLE = 0,
		ENABLE = 1,
		FRAMERATE = 2,
		FRAMENUM = 4,
		CAPTURES = 8
	};

	enum class CaptureOpts {
		VSYNC = 1,
		FULLSCREEN = 2,
		API_VALIDATION = 4,
		CAPTURE_CALLSTACKS = 8,
		VERIFY_BUFF_ACCESS = 16,
		REF_ALL_RES = 32,
		REDIRECT_DEBUG_OUT = 64
	};

	static RenderDocOverlayOptions operator|(RenderDocOverlayOptions l, RenderDocOverlayOptions r) {
		return (RenderDocOverlayOptions)((uint32_t)l | (uint32_t)r);
	}

	static uint32_t operator&(RenderDocOverlayOptions l, RenderDocOverlayOptions r) {
		return ((uint32_t)l & (uint32_t)r);
	}

	/// <summary>
	/// Wrapper for the RenderDoc API
	/// </summary>
	class Renderdoc {
	public :
		/// <summary>
		/// Loads the renderdoc shared object
		/// from the given path
		/// </summary>
		/// <param name="path">Path to the RenderDoc installation</param>
		Renderdoc(std::string const& path);

		/// <summary>
		/// Load renderdoc library
		/// </summary>
		void Load();

		/// <summary>
		/// Unloads the renderdoc library
		/// </summary>
		void Unload();

		/// <summary>
		/// Destroy this object (also unloads the
		/// RenderDoc library from process memory)
		/// </summary>
		~Renderdoc();

		/// <summary>
		/// Sets the current selected window
		/// </summary>
		/// <param name="win">Native handle to window</param>
		void SetCurrentWindow(void* win);

		/// <summary>
		/// Sets the current API context
		/// </summary>
		/// <param name="ctx">Handle to OpenGL context or Vulkan device</param>
		void SetCurrentContext(void* ctx) { m_curr_ctx = ctx; }

		/// <summary>
		/// Set the RenderDoc overlay options
		/// </summary>
		void SetOverlayOptions(RenderDocOverlayOptions opts);

		/// <summary>
		/// Set wanted capture option to val 
		/// </summary>
		/// <param name="opts">Capture option</param>
		/// <param name="val">Value (probably 0 or 1)</param>
		void SetCaptureOption(CaptureOpts opts, uint32_t val);

		/// <summary>
		/// Prepare API for capture
		/// </summary>
		void PrepareCapture() { m_capture_curr_frame = true; }

		void StartCapture();
		void EndCapture();

	private :
		std::string m_path;
		bool m_enabled;
		bool m_loaded;
		bool m_capture_curr_frame;
		std::unique_ptr<ModuleHandle> m_dll_hanlde;
		std::unique_ptr<ApiPointers> m_api_pointers;
		void* m_curr_ctx;
		void* m_curr_win;
	};
}