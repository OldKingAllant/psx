#include <psxemu/include/psxemu/Server.hpp>
#include <psxemu/include/psxemu/System.hpp>

#include <fmt/format.h>

namespace psx::gdbstub {
	static constexpr const char VERSION_STRING[] = "1.0";

	void Server::HandleExtVer(std::string&) {
		SendPayload(std::string_view{ VERSION_STRING, sizeof(VERSION_STRING) });
	}

	void Server::HandleExtDumpExceptionChains(std::string&) {
		SendPayload("OK");

		std::vector<psx::kernel::ExceptionChain> except_chains{
			m_sys->GetKernel().DumpAllExceptionChains()
		};

		fmt::println("{}", psx::kernel::FormatExceptionChains(except_chains));
	}

	void Server::InitExtHandlers() {
		m_ext_cmd_handlers.insert(std::pair{ std::string{"version"}, &Server::HandleExtVer });
		m_ext_cmd_handlers.insert(std::pair{ std::string{"except_chains"}, &Server::HandleExtDumpExceptionChains });
	}

	
}