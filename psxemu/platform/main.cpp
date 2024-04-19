#include <iostream>

#include <psxemu/include/psxemu/Server.hpp>
#include <psxemu/include/psxemu/System.hpp>
#include <psxemu/renderer/GLRenderer.hpp>

#include <usage/tty/TTY_Console.hpp>

#include <psxemu/renderer/SdlContext.hpp>
#include <psxemu/renderer/SdlWindow.hpp>
#include <psxemu/renderer/Renderdoc.hpp>

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

	renderdoc.SetCurrentWindow(vram_view.GetNativeWindowHandle());

	using SdlEvent = psx::video::SdlEvent;

	bool ov_enable = true;

	vram_view.Listen(SdlEvent::KeyPressed, [&renderdoc, &ov_enable](SdlEvent ev_type, std::any data) {
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
	});

	tty::TTY_Console console{ "../x64/Debug/TTY_Console.exe", "psx-tty" };

	console.Open();

	psx::System sys{};

	sys.LoadBios(std::string("../programs/SCPH1001.BIN"));
	sys.ResetVector();
	sys.ToggleBreakpoints(true);
	sys.EnableHLE(true);

	sys.SetPutchar([&console](char ch) {
		console.Putchar(ch);
	});

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

	while (server.HandlePackets() && vram_view.EventLoop())
	{
		if (!sys.Stopped()) {
			renderdoc.StartCapture();
			bool break_hit = sys.RunInterpreterUntilBreakpoint();
			renderdoc.EndCapture();

			if (break_hit) {
				sys.SetStopped(true);
				server.BreakTriggered();
			}
		}

		auto handle = sys.GetStatus()
			.sysbus->GetGPU()
			.GetRenderer()->GetFramebuffer()
			.GetInternalTexture();

		vram_view.Blit(handle);
	}

	server.Shutdown();

	console.Close();

	psx::video::SdlShutdown();
} 