#pragma once

#include <string>
#include <sstream>

namespace tty {
	struct ProgramInfo;

	/// <summary>
	/// Handler to a console process. The console process
	/// needs to be developed on its own
	/// </summary>
	class TTY_Console {
	public :
		/// <summary>
		/// Creates an instance
		/// </summary>
		/// <param name="program">Path of console program</param>
		/// <param name="pipe_name">Name of named pipe</param>
		TTY_Console(std::string program, std::string pipe_name);

		/// <summary>
		/// Whether to keep sent data in a buffer,
		/// to resend it in case the console process
		/// has been killed
		/// </summary>
		void SetKeepOld(bool keep) {
			m_old_out = "";
			m_keep_old = keep;
		}

		/// <summary>
		/// Flush all bytes to the console process
		/// instead of waiting for newline
		/// </summary>
		bool Flush();

		/// <summary>
		/// Whether the output buffer is flushed
		/// after every put function or only
		/// at newlines
		/// </summary>
		void SetAutoflush(bool autoflush) {
			m_autoflush = autoflush;
		}

		/// <summary>
		/// Attempts to start the console process
		/// </summary>
		/// <returns>True if process started successfully</returns>
		bool Open();

		/// <summary>
		/// Attempts to kill/close the console process
		/// </summary>
		/// <returns>True if process was killed succesfully</returns>
		bool Close();

		bool IsOpen() const {
			return m_open;
		}

		/// <summary>
		/// Send/buffer a single char
		/// </summary>
		/// <param name="ch">The char</param>
		/// <returns>Whether buffering/sending succeded</returns>
		bool Putchar(char ch);

		/// <summary>
		/// Sends up to len chars (if \0 is found before
		/// reaching the specified len, the function will
		/// stop)
		/// </summary>
		/// <param name="s">Pointer to string</param>
		/// <param name="len">Max len</param>
		/// <returns>Whether buffering/sending succeded</returns>
		bool Puts(const char* s, std::size_t len);

		~TTY_Console();

	private :
		bool SendImpl(std::string const& out);

	private :
		std::string m_program_name;
		std::string m_pipe_name;
		bool m_keep_old;
		bool m_autoflush;
		bool m_open;
		std::string m_old_out;
		std::ostringstream m_out_buf;
		ProgramInfo* m_prog_info;
	};
}