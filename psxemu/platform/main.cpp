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

#include <psxemu/include/psxemu/LoggerMacros.hpp>
#include <psxemu/include/psxemu/Logger.hpp>

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
	atexit(psx::video::SdlShutdown);

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

	DisplayWindow display("PSX-Display",
		psx::video::Rect{ .w = 640, .h = 480 },
		"../shaders", "display_blit", "display_blit24", true, true, true);

	if(renderdoc)
		renderdoc->SetCurrentWindow(display.GetNativeWindowHandle());

	using SdlEvent = psx::video::SdlEvent;

	bool ov_enable = true;

	std::shared_ptr<psx::input::IInputManager> input_manager{std::make_shared<psx::input::KeyboardManager>()};

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

	if (!config->cdrom_file.empty()) {
		if (!sys.InsertDisc(std::filesystem::path(config->cdrom_file))) {
			fmt::println("[CDROM] Could not load file");
		}
	}

	if (!config->exe_file.empty() && !config->patch_load) {
		auto args = config->exe_args.empty() ?
			std::nullopt :
			std::optional{ std::span{ 
			std::bit_cast<psx::u8*>( config->exe_args.data() ), 
			config->exe_args.size()
		} };
		sys.LoadExe(config->exe_file, args);
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

	std::shared_ptr<psx::gdbstub::Server> server{};
	server = std::make_shared<psx::gdbstub::Server>(config->gdb_stub_port, &sys);

	(void)server->SetTraceHandler([](std::string_view packet, bool inout) {
		if (inout) //Sending
			std::cout << "Out: " << packet;
		else
			std::cout << "In: " << packet;

		std::cout << std::endl;
	});
	server->SetTracing(false);

	if (config->enable_gdb_stub) {
		server->StartThread();
	}

	display.SetSystem(&sys);
	display.SetGdbServer(server);

	{
		//auto& mdec = sys.GetStatus()
		//	.sysbus->GetMDEC();
		//mdec.StartDecodeThread();
		//mdec.UseSimd();
		//mdec.UseInaccurateIdct();

		//auto& dma_control = sys.GetStatus()
		//	.sysbus->GetDMAControl();
		//dma_control.EnableFastDma(true);
		//dma_control.UseSimd(true);
	}

	if (debug_view) {
		debug_view->SetGdbServer(server);
	}

	sys.SetStopped(true);
	while (wm.HandleEvents())
	{
		bool debug_view_close_request = debug_view ? debug_view->CloseRequest() : false;

		if (display.CloseRequest() || debug_view_close_request) break;

		server->HandleAsyncCommands();

		if (!sys.Stopped()) {
			display.MakeContextCurrent();
			if(renderdoc) renderdoc->StartCapture();
			bool break_hit = sys.RunInterpreterUntilBreakpoint();
			if(renderdoc) renderdoc->EndCapture();

			if (break_hit || sys.Stopped()) {
				sys.SetStopped(true);
				server->SignalBreakpoint();
			}
		}

		auto& gpu = sys.GetStatus()
			.sysbus->GetGPU();

		auto& framebuf = gpu.GetRenderer()->GetFramebuffer();
		auto& vram = gpu.GetRenderer()->GetVram();
		auto handle = framebuf.GetUpscaledTexture().value_or(framebuf.GetInternalTexture());

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
				display.Blit24(vram.GetTextureHandle());
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

		display.DrawGui();
		display.Present();

		if (debug_view) {
			debug_view->Update();
		}
		
	}

	display.MakeContextCurrent();

	server->StopThread();
	if(console) console->Close();
} 