#pragma once

#include <Poco/Net/ServerSocket.h>

#include <map>
#include <optional>
#include <functional>

namespace psx {
	class System;
}

namespace psx::gdbstub {
	class Server;

	using CommandHandler = void(Server::*)(std::string&);

	/// <summary>
	/// Trace handler callback, where the string
	/// is the packet and bool is false when
	/// recv, true when send
	/// </summary>
	using TraceHandler = std::function<void(std::string_view, bool)>;

	class Server {
	public :
		Server(int16_t port, System* sys);

		/// <summary>
		/// Opens TCP connection on 
		/// the given port and waits
		/// for connection
		/// </summary>
		void Start();

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
		/// Performs a conversion from hex string to
		/// an unsigned 32 bit integer value
		/// </summary>
		/// <param name="str">Input string to convert</param>
		/// <param name="little">Use little endian</param>
		/// <returns>None if invalid input was found, else the converted value</returns>
		std::optional<uint32_t> HexStringToUint(std::string_view str, bool little) const;

		/// <summary>
		/// Converts 32 bits unsigned integer number to
		/// hex string, padding it to pad_to chars if
		/// the number is not big enough
		/// </summary>
		/// <param name="num">Number to convert</param>
		/// <param name="pad_to">Padding</param>
		/// <param name="little">Use little endian byte ordering</param>
		/// <returns>String representation</returns>
		std::string UintToHexString(uint32_t num, uint32_t pad_to, bool little) const;

		/// <summary>
		/// UNSAFE! Converts character
		/// to 4 bits nibble
		/// </summary>
		/// <param name="ch">The char</param>
		/// <returns>The nibble</returns>
		uint8_t HexToNibble(char ch) const;

		/// <summary>
		/// Performs the reverse operation
		/// wrt HexToNibble. However, this
		/// function never fails
		/// </summary>
		/// <param name="nibble"></param>
		/// <returns></returns>
		char NibbleToHex(uint8_t nibble) const;

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

		~Server();

		static constexpr int64_t TIMEOUT = 10000000;
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

		/// <summary>
		/// Server port
		/// </summary>
		int16_t m_port;

		/// <summary>
		/// If the connection is open
		/// </summary>
		bool m_open;

		char* m_recv_buffer;

		/// <summary>
		/// Last sent payload (not the complete packet)
		/// </summary>
		std::string m_out;

		std::size_t m_recv_size;

		std::map<std::string, CommandHandler> m_cmd_handlers;

		TraceHandler m_trace_handler;
		bool m_tracing;

		System* m_sys;
	};
}