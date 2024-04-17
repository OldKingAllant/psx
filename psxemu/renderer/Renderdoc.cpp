#include "Renderdoc.hpp"

#include <thirdparty/renderdoc/api/app/renderdoc_app.h>
#include <fmt/format.h>

#include <filesystem>

#include <Windows.h>

namespace psx::video {
	struct ApiPointers {
		RENDERDOC_API_1_6_0* api{nullptr};
	};

	struct ModuleHandle {
		HMODULE module;
	};

	Renderdoc::Renderdoc(std::string const& path) 
		: m_path{path}, m_enabled{}, m_loaded{}, 
		m_capture_curr_frame{}, m_dll_hanlde{nullptr},
		m_api_pointers{ nullptr }, m_curr_ctx{}, 
		m_curr_win{} {}

	Renderdoc::~Renderdoc() {
		Unload();
	}

	void Renderdoc::Load() {
		std::string complete_path{ m_path + "/renderdoc.dll" };

		if (!std::filesystem::exists(complete_path)) {
			fmt::println("[RENDERER] RenderDoc library not found");
			return;
		}

		HMODULE module = GetModuleHandleA(complete_path.c_str());

		if (!module) {
			fmt::println("[RENDERER] RenderDoc load failed!");
			return;
		}

		pRENDERDOC_GetAPI RENDERDOC_GetAPI{(pRENDERDOC_GetAPI)GetProcAddress(module, "RENDERDOC_GetAPI")};

		if (!RENDERDOC_GetAPI) {
			fmt::println("[RENDERER] Cannot find symbol RENDERDOC_GetAPI");
			return;
		}

		RENDERDOC_API_1_6_0* rdoc_api{ nullptr };

		int result = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_6_0, (void**)&rdoc_api);

		if (result != 1) {
			fmt::println("[RENDERER] RENDERDOC_GetAPI returned {}", result);
			return;
		}

		fmt::println("[RENDERER] RenderDoc loaded successfully");

		m_dll_hanlde = std::make_unique<ModuleHandle>(module);
		m_api_pointers = std::make_unique<ApiPointers>(rdoc_api);

		m_loaded = true;
	}

	void Renderdoc::Unload() {
		fmt::println("[RENDERER] RenderDoc API \'unload\'");

		//NOP, since unloading the API is impossible after
		//injection
	}

	void Renderdoc::StartCapture() {
		if (!m_loaded || !m_capture_curr_frame)
			return;

		if (m_api_pointers->api->IsFrameCapturing())
			return;

		m_api_pointers->api->StartFrameCapture(m_curr_ctx, m_curr_win);
	}

	void Renderdoc::EndCapture() {
		if (!m_loaded || !m_capture_curr_frame)
			return;

		if (!m_api_pointers->api->IsFrameCapturing())
			return;

		m_api_pointers->api->EndFrameCapture(m_curr_ctx, m_curr_win);
		m_capture_curr_frame = false;
	}

	void Renderdoc::SetOverlayOptions(RenderDocOverlayOptions opts) {
		if (!m_loaded)
			return;

		uint32_t options{ 0 };

		if (opts & RenderDocOverlayOptions::ENABLE)
			options |= eRENDERDOC_Overlay_Enabled;

		if (opts & RenderDocOverlayOptions::FRAMERATE)
			options |= eRENDERDOC_Overlay_FrameRate;

		if (opts & RenderDocOverlayOptions::FRAMENUM)
			options |= eRENDERDOC_Overlay_FrameNumber;

		if (opts & RenderDocOverlayOptions::CAPTURES)
			options |= eRENDERDOC_Overlay_CaptureList;

		m_api_pointers->api->MaskOverlayBits(options, options);
	}

	void Renderdoc::SetCurrentWindow(void* win) { 
		if (!m_loaded)
			return;

		m_curr_win = win; 

		m_api_pointers->api->SetActiveWindow(m_curr_ctx, m_curr_win);
	}

	void Renderdoc::SetCaptureOption(CaptureOpts opts, uint32_t val) {
		if (!m_loaded)
			return;

		RENDERDOC_CaptureOption effective_opt{};

		switch (opts)
		{
		case psx::video::CaptureOpts::VSYNC:
			effective_opt = eRENDERDOC_Option_AllowVSync;
			break;
		case psx::video::CaptureOpts::FULLSCREEN:
			effective_opt = eRENDERDOC_Option_AllowFullscreen;
			break;
		case psx::video::CaptureOpts::API_VALIDATION:
			effective_opt = eRENDERDOC_Option_APIValidation;
			break;
		case psx::video::CaptureOpts::CAPTURE_CALLSTACKS:
			effective_opt = eRENDERDOC_Option_CaptureCallstacks;
			break;
		case psx::video::CaptureOpts::VERIFY_BUFF_ACCESS:
			effective_opt = eRENDERDOC_Option_VerifyBufferAccess;
			break;
		case psx::video::CaptureOpts::REF_ALL_RES:
			effective_opt = eRENDERDOC_Option_RefAllResources;
			break;
		case psx::video::CaptureOpts::REDIRECT_DEBUG_OUT:
			effective_opt = eRENDERDOC_Option_DebugOutputMute;
			break;
		default:
			fmt::println("[RENDERER] Invalid RenderDoc capture option {}", (uint32_t)opts);
			return;
			break;
		}

		int result = m_api_pointers->api->SetCaptureOptionU32(effective_opt, val);

		if (!result) {
			fmt::println("[RENDERER] Invalid RenderDoc capture option value {} = {}", (uint32_t)opts,
				val);
		}
	}
}