#include <iostream>

#include <psxemu/include/psxemu/Server.hpp>
#include <psxemu/include/psxemu/System.hpp>
#include <psxemu/renderer/GLRenderer.hpp>

#include <usage/tty/TTY_Console.hpp>

#include <psxemu/renderer/SdlContext.hpp>
#include <psxemu/renderer/SdlWindow.hpp>

int main(int argc, char* argv[]) {
	psx::video::SdlInit();

	psx::video::SdlWindow vram_view("Vram",
		psx::video::Rect{ .w = 1024, .h = 512 }, 
		"../shaders", "vram_view_blit", false, false);

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
			bool break_hit = sys.RunInterpreterUntilBreakpoint();

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