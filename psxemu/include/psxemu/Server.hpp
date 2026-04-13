#pragma once

#include <Poco/Net/ServerSocket.h>
#include <Poco/Net/PollSet.h>

#include <map>
#include <optional>
#include <functional>
#include <thread>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <atomic>
#include <stop_token>

namespace psx {
	class System;
}

namespace psx::gdbstub {
	class Server;

	struct ServerCommand {
		ServerCommand() : server{nullptr} {}
		Server* server;
		virtual void Execute() = 0;
		virtual ~ServerCommand() {}
	};

	struct EmuCommand {
		EmuCommand() : server{nullptr} {}
		Server* server;
		virtual void Execute() = 0;
		virtual ~EmuCommand() {}
	};

	using CommandHandler = void(Server::*)(std::string&);

	/// <summary>
	/// Trace handler callback, where the string
	/// is the packet and bool is false when
	/// recv, true when send
	/// </summary>
	using TraceHandler = std::function<void(std::string_view, bool)>;

	class Server {
	public :
		friend struct ServerCommand;
		friend struct EmuCommand;

		Server(int16_t port, System* sys);

		void StartThread();
		void StopThread();

		//Call on emu thread
		void HandleAsyncCommands();

		//Call on server thread
		void HandleEmuCommands();

		template <typename CmdType, typename... Args>
		void PushServerToEmuCommand(Args&&... args) requires std::copy_constructible<CmdType> &&
		std::derived_from<CmdType, ServerCommand> {
			auto cmd = CmdType(std::forward<Args>(args)...);
			cmd.server = this;
			std::scoped_lock lk{ m_thread_state.server_queue_mutex };
			m_thread_state.server_to_emu.push_back(std::make_unique<CmdType>(cmd));
			m_thread_state.server_queue_cv.notify_one();
		}

		template <typename CmdType, typename... Args>
		void PushEmuToServerCommand(Args&&... args) requires std::copy_constructible<CmdType> &&
		std::derived_from<CmdType, EmuCommand> {
			auto cmd = CmdType(std::forward<Args>(args)...);
			cmd.server = this;
			std::scoped_lock lk{ m_thread_state.emu_queue_mutex };
			m_thread_state.emu_to_server.push_back(std::make_unique<CmdType>(cmd));
			m_thread_state.emu_queue_cv.notify_one();
		}

		template <typename CmdType>
		std::unique_ptr<CmdType> AwaitEmuResponse(std::stop_token token) requires 
			std::derived_from<CmdType, EmuCommand> {

			std::unique_ptr<CmdType> response{};

			while (!token.stop_requested() && !response) {
				std::unique_lock lk{ m_thread_state.emu_queue_mutex };
				if (m_thread_state.emu_to_server.empty()) {
					m_thread_state.emu_queue_cv.wait(lk, [this, token]() {
						return token.stop_requested() || !m_thread_state.emu_to_server.empty();
					});
				}

				std::unique_ptr<EmuCommand> cmd{};
				cmd.swap(m_thread_state.emu_to_server.front());
				m_thread_state.emu_to_server.pop_front();
				cmd->Execute();

				CmdType* derived = dynamic_cast<CmdType*>(cmd.get());
				if (derived == nullptr) {
					continue;
				}

				cmd.release();
				response = std::unique_ptr<CmdType>{ derived };
			}
			
			return response;
		}

		/// <summary>
		/// Opens TCP connection on 
		/// the given port and waits
		/// for connection
		/// </summary>
		void Start(std::stop_token token);

		/// <summary>
		/// Closes the connection
		/// and frees the associated
		/// resources
		/// </summary>
		void Shutdown();

		/// <summary>
		/// Handle incoming packets
		/// (if any)
		/// </summary>
		/// <returns>If the connection has been closed</returns>
		bool HandlePackets();

		/// <summary>
		/// Enables tracing of in/out packets
		/// </summary>
		/// <param name="trace">true => Trace/false => Don't trace</param>
		void SetTracing(bool trace) { m_tracing = trace; }

		/// <summary>
		/// Sets the packet trace handler
		/// </summary>
		/// <param name="handler">The new handler</param>
		/// <returns>The old trace handler</returns>
		TraceHandler SetTraceHandler(TraceHandler handler) {
			std::swap(m_trace_handler, handler);
			return handler;
		}

		/*
		All this methods shoulde be global, as
		they are general utilities. Unfortunately,
		I do not want to refactor this
		*/

		/// <summary>
		/// Computes a very short (1 byte) modulo 256 checksum
		/// from the given string. Useful for
		/// the gdbstub protocol. 
		/// </summary>
		/// <param name="string"></param>
		/// <returns>1 byte checksum</returns>
		uint8_t ComputeChecksum(std::string_view string);

		/// <summary>
		/// Tries to find a gdbstub separator 
		/// in the input string and returns
		/// the field separated by that separator
		/// with the position of the separator
		/// </summary>
		/// <param name="buf">Input string</param>
		/// <returns>None if no separator was found, else the field and  position of
		/// the separator</returns>
		std::optional<std::pair<std::string, std::size_t>> SeparateStr(std::string const& buf) const;

		/// <summary>
		/// Retrieves value of register with the given index
		/// from the state
		/// </summary>
		/// <param name="reg_index">Reg index</param>
		/// <returns>Reg value</returns>
		uint32_t GetRegValueFromIndex(uint8_t reg_index);

		/// <summary>
		/// Like GetRegValueFromIndex, but sets
		/// the value
		/// </summary>
		/// <param name="reg_index">Reg index</param>
		/// <param name="value">New value</param>
		void SetRegValueFromIndex(uint8_t reg_index, uint32_t value);

		/// <summary>
		/// Call on the server thread
		/// </summary>
		void BreakTriggered();

		/// <summary>
		/// Call on the emulation thread
		/// </summary>
		void SignalBreakpoint();

		inline bool IsConnected() const {
			return m_open.load();
		}

		inline bool IsThreadRunning() const {
			return m_thread_state.is_running.load();
		}

		inline int16_t GetPort() const {
			return m_port;
		}

		inline void SetPort(int16_t port) {
			if (m_thread_state.is_running) {
				return;
			}
			m_port = port;
		}

		inline Poco::Net::SocketAddress GetClientAddress() {
			std::scoped_lock lk{ m_address_lock };
			return m_address;
		}

		inline System* GetSystem() {
			return m_sys;
		}

		~Server();

		static constexpr int64_t TIMEOUT = 20000;
		static constexpr std::size_t BUFFER_SIZE = 2048;
		static constexpr const char SUPPORTED_STR[] = "PacketSize=1024;multiprocess-;qRelocInsn-;hwbreak+;vContSupported+";
		static constexpr std::size_t NUM_STUB_REGISTERS = 73;

	private :
		/// <summary>
		/// Handles the qSupported GDB command.
		/// This handler will likely be called
		/// once at the beginning and no more
		/// </summary>
		/// <param name="data">The command data</param>
		void HandleQSupported(std::string& data);
		void HandleHg(std::string& data);
		void HandleQAttached(std::string& data);
		void HandleQuestionMark(std::string& data);
		void HandleHc(std::string& data);
		void HandleQC(std::string& data);
		void HandleQfThreadInfo(std::string& data);
		void HandleQsThreadInfo(std::string& data);
		void HandleSmallG(std::string& data);
		void HandleBigG(std::string& data);
		void HandleDetach(std::string& data);
		void HandleP(std::string& data);
		void HandleM(std::string& data);
		void HandleBigM(std::string& data);
		void HandleVContQuestionMark(std::string& data);
		void HandleVCont(std::string& data);
		void HandleZ1(std::string& data);
		void HandleSmallZ1(std::string& data);

		/// <summary>
		/// Handles unknown/unsopprted command 
		/// by sending an empty packet
		/// </summary>
		/// <param name="name">Command name</param>
		/// <param name="data">Command data</param>
		void HandleUnkownCommand(std::string const& name, std::string const& data);

		/// <summary>
		/// Sends empty response +$#00
		/// </summary>
		void SendEmpty();

		/// <summary>
		/// Effectively sends data to the GDB client
		/// </summary>
		/// <param name="payload">Payload to send</param>
		void SendPayload(std::string_view payload);

		/// <summary>
		/// Sends a single char '-' to
		/// request retransmission of last packet
		/// </summary>
		void SendNAck();

		/// <summary>
		/// Immediately send ACK when recvd
		/// valid packet, before packet 
		/// processing
		/// </summary>
		void SendAck();

		/// <summary>
		/// Handle packets implemented only
		/// by this stub
		/// </summary>
		/// <param name="data">Sub-command</param>
		void HandleExtensionPackets(std::string& data);

		/// <summary>
		/// Call this after object
		/// creation (in constructor)
		/// </summary>
		void InitExtHandlers();

		/// <summary>
		/// Handlers for extension packets
		/// </summary>

		void HandleExtVer(std::string&);
		void HandleExtDumpExceptionChains(std::string&);
		void HandleExtTimestamp(std::string&);

		void ThreadMain(std::stop_token token);

	private :
		/// <summary>
		/// Server socket (used only for accepting initial connection from GDB client)
		/// </summary>
		Poco::Net::ServerSocket m_socket;

		/// <summary>
		/// Connection socket to client
		/// </summary>
		Poco::Net::StreamSocket m_conn;

		/// <summary>
		/// Client address
		/// </summary>
		Poco::Net::SocketAddress m_address;
		std::mutex m_address_lock;

		/// <summary>
		/// Server port
		/// </summary>
		int16_t m_port;

		/// <summary>
		/// If the connection is open
		/// </summary>
		std::atomic<bool> m_open;

		std::unique_ptr<char[]> m_recv_buffer;

		/// <summary>
		/// Last sent payload (not the complete packet)
		/// </summary>
		std::string m_out;

		std::size_t m_recv_size;

		std::map<std::string, CommandHandler> m_cmd_handlers;
		std::map<std::string, CommandHandler> m_ext_cmd_handlers;

		TraceHandler m_trace_handler;
		bool m_tracing;

		System* m_sys;

		struct ThreadState {
			std::jthread server_thread;
			//Server to emu thread commands
			std::deque<std::unique_ptr<ServerCommand>> server_to_emu;
			//Emu thread to server commands
			std::deque<std::unique_ptr<EmuCommand>> emu_to_server;
			//Server to emu mutex
			std::mutex server_queue_mutex;
			//Emu to server mutex
			std::mutex emu_queue_mutex;
			//Server to emu cv
			std::condition_variable server_queue_cv;
			//Emu to server cv
			std::condition_variable emu_queue_cv;
			std::atomic<bool> is_running;
		};

		ThreadState m_thread_state;
	};
}