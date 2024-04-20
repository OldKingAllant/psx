#include <psxemu/include/psxemu/Server.hpp>

namespace psx::gdbstub {
	static constexpr const char VERSION_STRING[] = "1.0";

	void Server::HandleExtVer(std::string&) {
		SendPayload(std::string_view{ VERSION_STRING, sizeof(VERSION_STRING) });
	}

	void Server::InitExtHandlers() {
		m_ext_cmd_handlers.insert(std::pair{ std::string{"version"}, &Server::HandleExtVer });
	}
}