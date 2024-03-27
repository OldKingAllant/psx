#include "TTY_Console.hpp"

#include <filesystem>

#include <Windows.h>

namespace tty {
	struct ProgramInfo {
		PROCESS_INFORMATION proc_info;
		HANDLE pipe_handle;
	};

	TTY_Console::TTY_Console(std::string program, std::string pipe_name) :
		m_program_name{program}, m_pipe_name{pipe_name},
		m_keep_old {false}, m_autoflush{false}, m_open{false}, 
		m_old_out{}, m_out_buf{}, 
		m_prog_info{nullptr} {
		m_prog_info = new ProgramInfo{};
	}

	bool TTY_Console::Open() {
		if (m_open)
			return false;

		if (!std::filesystem::exists(m_program_name))
			return false;

		std::string effective_pipe = R"(\\.\pipe\)" +
			m_pipe_name;

		std::string cmd_line = m_program_name 
			+ " " + effective_pipe;

		std::string win_title = m_pipe_name + "\0";

		STARTUPINFOA start_info{};

		std::memset(&start_info, 0, sizeof(start_info));
		std::memset(&m_prog_info->proc_info, 0, sizeof(PROCESS_INFORMATION));

		start_info.cb = sizeof(STARTUPINFOA);
		start_info.lpTitle = win_title.data();

		auto pipe = CreateNamedPipeA(
			effective_pipe.c_str(),
			PIPE_ACCESS_DUPLEX,
			PIPE_TYPE_MESSAGE,
			2,
			1024,
			1024,
			INFINITE,
			NULL
		);

		if (pipe == INVALID_HANDLE_VALUE)
			return false;

		m_prog_info->pipe_handle = pipe;

		auto result = CreateProcessA(
			m_program_name.c_str(),
			cmd_line.data(),
			NULL,
			NULL,
			FALSE,
			NORMAL_PRIORITY_CLASS | CREATE_NEW_CONSOLE,
			NULL,
			NULL,
			&start_info,
			&m_prog_info->proc_info
		);

		if (!result)
			return false;

		auto connected = ConnectNamedPipe(
			pipe, NULL
		);

		if (!connected && GetLastError() != ERROR_PIPE_CONNECTED)
			return false;

		m_open = true;

		if (m_keep_old && !m_old_out.empty()) {
			SendImpl(m_old_out);
		}

		return true;
	}

	bool TTY_Console::Close() {
		if (!m_open)
			return false;

		m_open = false;

		auto ok = TerminateProcess(m_prog_info->proc_info.hProcess, 0);

		if (!ok)
			return false;

		(void)CloseHandle(m_prog_info->pipe_handle);

		return true;
	}

	bool TTY_Console::Flush() {
		if (!m_open)
			return false;

		std::string out = m_out_buf.str();

		if (out.empty())
			return true;

		m_out_buf = {};

		if (m_keep_old)
			m_old_out += out;

		return SendImpl(out);
	}

	bool TTY_Console::SendImpl(std::string const& out) {
		DWORD exit_code{};

		auto res = GetExitCodeProcess(m_prog_info->proc_info.hProcess, &exit_code);

		if (!res || exit_code != STILL_ACTIVE)
			return false;

		DWORD written_bytes{};

		auto res2 = WriteFile(
			m_prog_info->pipe_handle, 
			out.c_str(), 
			(DWORD)out.size(), 
			&written_bytes, 
			NULL
		);

		return (bool)res2;
	}

	bool TTY_Console::Putchar(char ch) {
		m_out_buf << ch;

		if (ch == '\n' || m_autoflush)
			return Flush();

		return true;
	}

	bool TTY_Console::Puts(const char* s, std::size_t len) {
		bool flush = false;

		while (*s && len--) {
			if (*s == '\n')
				flush = true;

			m_out_buf << *s;
			s++;
		}

		if (flush || m_autoflush)
			return Flush();

		return true;
	}

	TTY_Console::~TTY_Console() {
		if (m_open)
			Close();

		if (m_prog_info)
			delete m_prog_info;

		m_open = false;
		m_prog_info = nullptr;
	}
}