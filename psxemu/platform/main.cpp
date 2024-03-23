#include <iostream>

#include <psxemu/include/psxemu/Server.hpp>
#include <psxemu/include/psxemu/System.hpp>

int main(int argc, char* argv[]) {
	psx::System sys{};

	sys.LoadBios(std::string("../programs/SCPH1001.BIN"));
	sys.ResetVector();
	sys.ToggleBreakpoints(true);

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

	while (server.HandlePackets())
	{}

	server.Shutdown();
} 