#include <iostream>

#include <psxemu/include/psxemu/Server.hpp>
#include <psxemu/include/psxemu/System.hpp>
#include <psxemu/renderer/GLRenderer.hpp>

#include <usage/tty/TTY_Console.hpp>

#include <psxemu/renderer/SdlContext.hpp>
#include <psxemu/renderer/SdlWindow.hpp>
#include <psxemu/renderer/Renderdoc.hpp>
#include <psxemu/renderer/WindowManager.hpp>
#include <usage/imgui_debug/DebugView.hpp>

int main(int argc, char* argv[]) {
	psx::video::SdlInit();

	char* buf{ nullptr };
	size_t len{ 0 };
	auto err = _dupenv_s(&buf, &len, "PROGRAMFILES");

	if (err || buf == nullptr) {
		std::cerr << "Program files env variable not defined??";
		std::exit(0);
	}

	if (buf[len - 1] == '\0')
		len -= 1;

	std::string prog_files{ buf, len };
	free(buf);
	std::cout << "Program files path : " << prog_files << std::endl;
	std::string possible_renderdoc_path{ prog_files + "/RenderDoc/" };

	psx::video::Renderdoc renderdoc{ possible_renderdoc_path };

	using OverlayOpts = psx::video::RenderDocOverlayOptions;
	using CaptureOpts = psx::video::CaptureOpts;

	renderdoc.Load();
	renderdoc.SetCaptureOption(CaptureOpts::API_VALIDATION, 1);
	renderdoc.SetCaptureOption(CaptureOpts::REDIRECT_DEBUG_OUT, 0);
	renderdoc.SetCaptureOption(CaptureOpts::VERIFY_BUFF_ACCESS, 1);
	renderdoc.SetCaptureOption(CaptureOpts::REF_ALL_RES, 1);
	renderdoc.SetCaptureOption(CaptureOpts::CAPTURE_CALLSTACKS, 1);

	psx::video::SdlWindow vram_view("Vram",
		psx::video::Rect{ .w = 1024, .h = 512 }, 
		"../shaders", "vram_view_blit", false, true);

	psx::video::SdlWindow display("PSX-Display",
		psx::video::Rect{ .w = 640, .h = 480 },
		"../shaders", "display_blit", true, true);

	renderdoc.SetCurrentWindow(display.GetNativeWindowHandle());

	using SdlEvent = psx::video::SdlEvent;

	bool ov_enable = true;

	vram_view.Listen(SdlEvent::KeyPressed, [&renderdoc, &ov_enable, &vram_view](SdlEvent ev_type, std::any data) {
		auto key_name = std::any_cast<std::string_view>(data);
		
		if (key_name == "C")
			renderdoc.PrepareCapture();
		else if (key_name == "O") {
			ov_enable = !ov_enable;

			fmt::println("Renderdoc overlay enable : {}", ov_enable);

			if(!ov_enable)
				renderdoc.SetOverlayOptions(OverlayOpts::DISABLE);
			else 
				renderdoc.SetOverlayOptions(OverlayOpts::ENABLE | OverlayOpts::FRAMERATE | OverlayOpts::CAPTURES);
		}
		else if (key_name == "R") {
			vram_view.SetSize(psx::video::Rect{.w = 1024, .h = 512});
		}
	});

	psx::video::WindowManager wm{};

	wm.AddWindow(&vram_view);
	wm.AddWindow(&display);

	tty::TTY_Console console{ "../x64/Release/TTY_Console.exe", "psx-tty" };

	console.Open();

	psx::System sys{};

	sys.LoadBios(std::string("../programs/SCPH1001.BIN"));
	sys.ResetVector();
	sys.ToggleBreakpoints(true);
	sys.SetHleEnable(true);
	sys.SetEnableKernelCallstack(true);

	psx::kernel::Kernel& kernel = sys.GetKernel();

	kernel.SetHooksEnable(true);
	kernel
		.InsertEnterHook(std::string("putchar"),
			[&console, &sys](psx::u32 pc, psx::u32 id) {
				psx::u32 ch = sys.GetCPU().GetRegs().a0;
				console.Putchar((char)ch);
			});

	DebugView debug_view{ std::make_shared<psx::video::SdlWindow>(
		std::string("Debug view"), psx::video::Rect{ .w = 1200, .h = 800 }, 
		true, true
	), &sys };

	wm.AddWindow(debug_view.GetRawWindow());
	wm.SetWindowAsUnfiltered(debug_view.GetRawWindow());

	psx::gdbstub::Server server(5000, &sys);

	(void)server.SetTraceHandler([](std::string_view packet, bool inout) {
		if (inout) //Sending
			std::cout << "Out: " << packet;
		else
			std::cout << "In: " << packet;

		std::cout << std::endl;
	});
	server.SetTracing(false);

	server.Start();

	std::cout << "Connected" << std::endl;

	while (server.HandlePackets() && wm.HandleEvents())
	{
		if (vram_view.CloseRequest() || display.CloseRequest() || debug_view.CloseRequest()) break;

		if (!sys.Stopped()) {
			renderdoc.StartCapture();
			bool break_hit = sys.RunInterpreterUntilBreakpoint();
			renderdoc.EndCapture();

			if (break_hit || sys.Stopped()) {
				sys.SetStopped(true);
				server.BreakTriggered();
			}
		}

		auto& gpu = sys.GetStatus()
			.sysbus->GetGPU();

		auto handle = gpu.GetRenderer()
			->GetFramebuffer()
			.GetInternalTexture();

		vram_view.Blit(handle);
		vram_view.Present();

		auto const& disp_conf = gpu.GetDispConfig();

		if (!disp_conf.display_enable) {
			display.Clear();
		}
		else {
			display.SetTextureWindow(
				disp_conf.disp_x,
				disp_conf.disp_y,
				psx::video::Rect{ .w = disp_conf.hoz_res - 1, .h = disp_conf.vert_res - 1 },
				psx::video::Rect{ .w = 1024, .h = 512 }
			);
			display.Blit(handle);
		}

		display.Present();
		debug_view.Update();
	}

	server.Shutdown();

	console.Close();

	psx::video::SdlShutdown();
} 