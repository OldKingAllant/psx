#include <iostream>

#include <psxemu/include/psxemu/Server.hpp>
#include <psxemu/include/psxemu/System.hpp>
#include <psxemu/include/psxemu/SIOPadMemcardDriver.hpp>
#include <psxemu/renderer/GLRenderer.hpp>

#include <usage/tty/TTY_Console.hpp>

#include <psxemu/renderer/SdlContext.hpp>
#include <psxemu/renderer/SdlWindow.hpp>
#include <psxemu/renderer/Renderdoc.hpp>
#include <psxemu/renderer/WindowManager.hpp>
#include <psxemu/input/KeyboardManager.hpp>

#include <usage/imgui_debug/DebugView.hpp>
#include <usage/config/ConfigLoader.hpp>
#include <usage/display/DisplayWindow.hpp>

std::string GetRenderdocPath() {
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
	return prog_files + "/RenderDoc/";
}

int main(int argc, char* argv[]) {
	psx::video::SdlInit();

	config::Config config_loader{};
	config_loader.LoadFromFile("../config/emu_conf.txt");
	auto config = config_loader.GetSystemConfig();

	using OverlayOpts = psx::video::RenderDocOverlayOptions;
	using CaptureOpts = psx::video::CaptureOpts;

	std::unique_ptr<psx::video::Renderdoc> renderdoc{};

	if (config->load_renderdoc) {
		auto possible_renderdoc_path = GetRenderdocPath();
		renderdoc.reset(new psx::video::Renderdoc{ possible_renderdoc_path });

		renderdoc->Load();
		renderdoc->SetCaptureOption(CaptureOpts::API_VALIDATION, 1);
		renderdoc->SetCaptureOption(CaptureOpts::REDIRECT_DEBUG_OUT, 0);
		renderdoc->SetCaptureOption(CaptureOpts::VERIFY_BUFF_ACCESS, 1);
		renderdoc->SetCaptureOption(CaptureOpts::REF_ALL_RES, 1);
		renderdoc->SetCaptureOption(CaptureOpts::CAPTURE_CALLSTACKS, 1);
	}

	psx::video::SdlWindow vram_view("Vram",
		psx::video::Rect{ .w = 1024, .h = 512 }, 
		"../shaders", "vram_view_blit", false, true, true);

	DisplayWindow display("PSX-Display",
		psx::video::Rect{ .w = 640, .h = 480 },
		"../shaders", "display_blit", "display_blit24", true, true, true);

	if(renderdoc)
		renderdoc->SetCurrentWindow(vram_view.GetNativeWindowHandle());

	using SdlEvent = psx::video::SdlEvent;

	bool ov_enable = true;

	std::shared_ptr<psx::input::IInputManager> input_manager{std::make_shared<psx::input::KeyboardManager>()};

	vram_view.Listen(SdlEvent::KeyPressed, [&renderdoc, &ov_enable, &vram_view](SdlEvent ev_type, std::any data) {
		auto key_name = std::any_cast<std::string_view>(data);
		
		if (renderdoc) {
			if (key_name == "C")
				renderdoc->PrepareCapture();
			else if (key_name == "O") {
				ov_enable = !ov_enable;

				fmt::println("Renderdoc overlay enable : {}", ov_enable);

				if (!ov_enable)
					renderdoc->SetOverlayOptions(OverlayOpts::DISABLE);
				else
					renderdoc->SetOverlayOptions(OverlayOpts::ENABLE | OverlayOpts::FRAMERATE | OverlayOpts::CAPTURES);
			}
		}

		if (key_name == "R") {
			vram_view.SetSize(psx::video::Rect{.w = 1024, .h = 512});
		}
	});

	display.Listen(SdlEvent::KeyPressed, [input_manager](SdlEvent ev_type, std::any data) {
		auto key_name = std::any_cast<std::string_view>(data);

		using ButtonStatus = psx::input::KeyboardButtonStatus;

		ButtonStatus status{ .key = key_name, .pressed = true };

		input_manager->Deliver(std::any{ status });
	});

	display.Listen(SdlEvent::KeyReleased, [input_manager](SdlEvent ev_type, std::any data) {
		auto key_name = std::any_cast<std::string_view>(data);

		using ButtonStatus = psx::input::KeyboardButtonStatus;

		ButtonStatus status{ .key = key_name, .pressed = false };

		input_manager->Deliver(std::any{ status });
	});

	psx::video::WindowManager wm{};

	wm.AddWindow(&vram_view);
	wm.AddWindow(&display);

	std::unique_ptr<tty::TTY_Console> console{};

	if (config->show_tty) {
		console.reset(new tty::TTY_Console{ config->tty_program, "psx-tty" });
		console->Open();
	}

	psx::System sys{config};

	psx::kernel::Kernel& kernel = sys.GetKernel();

	kernel
		.InsertEnterHook(std::string("putchar"),
			[&console, &sys](psx::u32 pc, psx::u32 id) {
				psx::u32 ch = sys.GetCPU().GetRegs().a0;
				if(console) console->Putchar((char)ch);
			});
	kernel
		.InsertEnterHook(std::string("SystemError"),
			[&sys](psx::u32 pc, psx::u32 id) {
				auto const& regs = sys.GetCPU().GetRegs();
				auto type = char(regs.a0);
				auto errcode = regs.a1;

				auto errmessage = fmt::format("SystemError called\nType: {}, Code: {:#x}",
					type, errcode);

				MessageBoxA(NULL, errmessage.c_str(),
					"Fatal error", MB_OK | MB_ICONERROR);
			});

	if (renderdoc) {
		sys.GetStatus()
			.sysbus->GetGPU()
			.GetRenderer()->SetRenderdocAPI(renderdoc.get());
	}
	
	auto controller = dynamic_cast<psx::SIOPadCardDriver*>(
		sys.GetStatus().sysbus->GetSIO0()
		.GetDevice1()
	)->GetController();

	if (config->controller_1_connected) {
		input_manager->AttachController(controller);
	}

	if (!config->controller_1_map.empty()) {
		input_manager->SetKeyMap(config->controller_1_map);
	}

	sys.LoadExe(std::string("../programs/mdec/movie/movie-15bit.exe"), std::nullopt);
	//sys.LoadExe(std::string("../programs/mdec/frame/frame-15bit-dma.exe"), std::nullopt);

	if (!config->cdrom_file.empty()) {
		if (!sys.InsertDisc(std::filesystem::path(config->cdrom_file))) {
			fmt::println("[CDROM] Could not load file");
		}
	}

	std::unique_ptr<DebugView> debug_view{};

	if (config->show_debug_win) {
		auto internal_window = std::make_shared<psx::video::SdlWindow>(
			std::string("Debug view"), psx::video::Rect{ .w = 1200, .h = 800 },
			true, true
		);

		debug_view.reset(new DebugView{internal_window , &sys});

		wm.AddWindow(debug_view->GetRawWindow());
		wm.SetWindowAsUnfiltered(debug_view->GetRawWindow());
	}

	psx::gdbstub::Server server(config->gdb_stub_port, &sys);

	(void)server.SetTraceHandler([](std::string_view packet, bool inout) {
		if (inout) //Sending
			std::cout << "Out: " << packet;
		else
			std::cout << "In: " << packet;

		std::cout << std::endl;
	});
	server.SetTracing(false);
	server.Start();

	{
		auto& mdec = sys.GetStatus()
			.sysbus->GetMDEC();
		mdec.StartDecodeThread();
		mdec.UseSimd();
		//mdec.UseInaccurateIdct();
	}

	while (server.HandlePackets() && wm.HandleEvents())
	{
		bool debug_view_close_request = debug_view ? debug_view->CloseRequest() : false;

		if (vram_view.CloseRequest() || display.CloseRequest() || debug_view_close_request) break;

		if (!sys.Stopped()) {
			display.MakeContextCurrent();
			if(renderdoc) renderdoc->StartCapture();
			bool break_hit = sys.RunInterpreterUntilBreakpoint();
			if(renderdoc) renderdoc->EndCapture();

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
			auto disp_range = gpu.ComputeDisplayRange();

			using ColorDepth = psx::DisplayColorDepth;

			if (gpu.GetStat().disp_color_depth == ColorDepth::BITS24) {
				display.SetTextureWindow24(
					disp_conf.disp_x,
					disp_conf.disp_y,
					psx::video::Rect{ .w = disp_range.xsize, .h = disp_range.ysize },
					psx::video::Rect{ .w = 1024, .h = 512 }
				);
				display.Blit24(handle);
			}
			else {
				display.SetTextureWindow(
					disp_conf.disp_x,
					disp_conf.disp_y,
					psx::video::Rect{ .w = disp_range.xsize, .h = disp_range.ysize },
					psx::video::Rect{ .w = 1024, .h = 512 }
				);
				display.Blit(handle);
			}
		}

		display.Present();

		if (debug_view) {
			debug_view->Update();
		}
		
	}

	server.Shutdown();
	if(console) console->Close();

	psx::video::SdlShutdown();
} 