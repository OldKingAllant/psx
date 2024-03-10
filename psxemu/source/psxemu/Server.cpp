#include <psxemu/include/psxemu/Server.hpp>
#include <Poco/Net/SocketStream.h>

#include <chrono>
#include <algorithm>
#include <sstream>

#include <psxemu/include/psxemu/System.hpp>
#include <psxemu/include/psxemu/SystemBus.hpp>

namespace psx::gdbstub {
	Server::Server(int16_t port, System* sys) :
		m_socket{},
		m_conn{},
		m_address{},
		m_port{port},
		m_open{false},
		m_recv_buffer{nullptr},
		m_out{},
		m_recv_size{},
		m_cmd_handlers{},
		m_trace_handler{}, m_tracing{false}, 
		m_sys{sys} {
		m_cmd_handlers.insert(std::pair{ std::string("qSupported"), &Server::HandleQSupported });
		m_cmd_handlers.insert(std::pair{ std::string("Hg"), &Server::HandleHg });
		m_cmd_handlers.insert(std::pair{ std::string("qAttached"), &Server::HandleQAttached });
		m_cmd_handlers.insert(std::pair{ std::string("?"), &Server::HandleQuestionMark });
		m_cmd_handlers.insert(std::pair{ std::string("Hc"), &Server::HandleHg });
		m_cmd_handlers.insert(std::pair{ std::string("qC"), &Server::HandleQC });
		m_cmd_handlers.insert(std::pair{ std::string("qfThreadInfo"), &Server::HandleQfThreadInfo });
		m_cmd_handlers.insert(std::pair{ std::string("qsThreadInfo"), &Server::HandleQsThreadInfo });
		m_cmd_handlers.insert(std::pair{ std::string("g"), &Server::HandleSmallG });
		m_cmd_handlers.insert(std::pair{ std::string("G"), &Server::HandleBigG });
		m_cmd_handlers.insert(std::pair{ std::string("D"), &Server::HandleDetach });
		m_cmd_handlers.insert(std::pair{ std::string("P"), &Server::HandleP });
		m_cmd_handlers.insert(std::pair{ std::string("m"), &Server::HandleM });
	}

	void Server::Start() {
		auto server_address = Poco::Net::SocketAddress("127.0.0.1", m_port);;

		m_socket.bind(server_address);

		using namespace std::chrono_literals;

		m_socket.listen(1);
		m_conn = m_socket.acceptConnection();
		
		/*m_conn.setBlocking(true);
		m_conn.setReceiveTimeout(Poco::Timespan(
			TIMEOUT
		));*/

		m_recv_buffer = new char[BUFFER_SIZE];

		std::memset(m_recv_buffer, 0x0, BUFFER_SIZE);

		m_address = m_conn.peerAddress();

		m_open = true;
	}

	void Server::Shutdown() {
		if (m_open) {
			m_conn.close();
			m_socket.close();
		}
	}

	bool Server::HandlePackets() {
		//The packet format is specified at https://sourceware.org/gdb/current/onlinedocs/gdb.html/Overview.html#Overview
		try {
			auto effective_len = m_conn.receiveBytes(m_recv_buffer, BUFFER_SIZE);

			if (effective_len == 0) //Returned with no data, probably closed connection?
				return false;

			m_recv_size = effective_len;

			std::string data{ m_recv_buffer, (std::size_t)effective_len };

			if (m_tracing && m_trace_handler)
				m_trace_handler(data, false);

			//The connection might be closed by a command
			//and a single packet might contain more
			//than one command
			while (data.length() && m_open) {
				if (data[0] == '+' || data[0] == '\0') {
					data.erase(0, 1);
				}
				else if (data[0] == '-') {
					//SendPayload(m_out);
					data.erase(0, 1);
				}
				else {
					if (data[0] != '$')
						return true;

					data.erase(0, 1);

					std::size_t start = 0;
					auto end = data.find_first_of('#');

					if (start == std::string::npos || end == std::string::npos)
						return true;

					//Last 2 chars in a packet
					auto checksum_str = std::string_view(data.begin() + end + 1,
						data.begin() + end + 3);

					//Data inside the leading $ and trailing #
					auto payload = std::string_view(data.begin() + start, data.begin() + end);
					auto computed_checksum = ComputeChecksum(payload); //Our checksum
					auto converted_checksum = HexStringToUint(checksum_str, false); //Inbound checksum

					if (!converted_checksum.has_value())
						return true;

					if (computed_checksum != converted_checksum.value()) {
						SendNAck(); //Request retransmission
					}
					else {
						std::optional<std::pair<std::string, std::size_t>> command = std::nullopt;

						//Hope that the command is split 
						//from its data in a way that
						//makes sense.
						//If you look at the GDB docs, this
						//is often not true
						//auto command = SeparateStr(data);
						//No clear separation
						auto payload_cmd = std::string(payload);

						//First try to match the entire payload
						if (m_cmd_handlers.contains(payload_cmd)) {
							command = std::pair{ payload_cmd, end };
						}
						else {
							//Try to match commands one char
							//at a time
							std::string cmd_builder = "";

							cmd_builder.push_back(data[0]);

							bool match = false;

							for (std::size_t pos = 1; pos < end && !match; pos++) {
								if (m_cmd_handlers.contains(cmd_builder)) {
									match = true;
									command = std::pair{ cmd_builder, pos - 1 };
								}
								else
									cmd_builder.push_back(data[pos]);
							}

							if (!match)
								command = std::pair{ cmd_builder, 0 };
						}

						//Command itself + start of data
						auto [the_command, offset] = command.value();

						std::string command_data = "";

						if (offset >= end)
							command_data = "";
						else
							command_data = std::string(data.begin() + offset + 1,
							data.begin() + end);

						if (m_cmd_handlers.contains(the_command)) {
							auto handler = m_cmd_handlers[the_command];

							std::invoke(handler, this, command_data);
						}
						else {
							HandleUnkownCommand(the_command, command_data);
						}
					}

					//Skip checksum
					data.erase(0, end + 2);
				}
			}
		}
		catch (std::exception const&) {
			return false;
		}

		return true;
	}

	std::optional<std::pair<std::string, std::size_t>> Server::SeparateStr(std::string const& buf) const {
		std::size_t offset = 0;

		while (offset < buf.length() &&
			buf[offset] != ',' &&
			buf[offset] != ':' &&
			buf[offset] != ';' &&
			buf[offset] != '-')
			offset++;

		if (offset >= buf.length())
			return std::nullopt;

		std::string part(buf.begin(), buf.begin() + offset);

		return std::pair{ part, offset };
	}

	void Server::SendPayload(std::string_view payload) {
		//Directly encode payload
		m_out = payload;

		std::string out_str = "";

		std::ostringstream stream{};

		stream << "+"; //Ack
		stream << "$";
		stream << payload;
		stream << "#";

		uint8_t checksum = ComputeChecksum(payload);

		stream << UintToHexString(checksum, 2, false);

		stream << "\0"; //Add leading \0 just to be sure

		out_str = stream.str();

		if (m_tracing && m_trace_handler)
			m_trace_handler(out_str, true);

		m_conn.sendBytes(out_str.data(), (int)out_str.size());
	}

	void Server::HandleUnkownCommand(std::string const& name, std::string const& data) {
		std::cout << "Invalid/Unknown command encountered : " << name << std::endl;
		
		//The docs say that we should send 
		//an empty response
		SendEmpty();
	}

	void Server::HandleQSupported(std::string& data) {
		//No need to evaluate inbound data
		SendPayload(SUPPORTED_STR);
	}

	uint8_t Server::ComputeChecksum(std::string_view str) {
		uint32_t checksum = 0;

		for (char ch : str)
			checksum = (checksum + (uint32_t)ch) % 256;

		return (uint8_t)checksum;
	}

	void Server::SendEmpty() {
		SendPayload("");
	}

	void Server::SendNAck() {
		m_out = "-";

		m_conn.sendBytes(m_out.c_str(), (int)m_out.size());
	}

	uint8_t Server::HexToNibble(char ch) const {
		//Assume char is in valid range
		//and uppercase
		if (ch >= '0' && ch <= '9')
			return ch - '0';
		else
			return ch - 'A' + 10;
	}

	std::optional<uint32_t> Server::HexStringToUint(std::string_view str, bool little) const {
		uint32_t num = 0;

		//Yes, there are thousands of more clever
		//ways of implementing this instead of using
		//two different loops
		if (little) {
			uint8_t shift = 0;

			for (std::size_t index = 0; index < str.length(); index++) {
				char ch = std::toupper(str[index]);

				if (!std::isalnum(ch) || ch > 'F')
					return std::nullopt;

				uint32_t nibble = HexToNibble(ch);

				if (index & 1) {
					//Low nibble
					num |= nibble << shift;
					shift += 8;
				}
				else
					num |= nibble << (shift + 4); //High nibble
			}
		}
		else {
			for (std::size_t index = 0; index < str.length(); index++) {
				char ch = std::toupper(str[index]);

				if (!std::isalnum(ch) || ch > 'F')
					return std::nullopt;

				uint32_t nibble = HexToNibble(ch);

				num |= nibble;

				if (index != str.length() - 1)
					num <<= 4; //Simply push to the left
			}
		}

		return num;
	}

	char Server::NibbleToHex(uint8_t nibble) const {
		if (nibble <= 9)
			return '0' + nibble;
		else
			return 'a' + (nibble - 10);
	}

	std::string Server::UintToHexString(uint32_t num, uint32_t pad_to, bool little) const {
		if (pad_to == 0)
			return "";

		std::string ret(pad_to, '0');

		std::size_t original_pad = pad_to;

		if (little) {
			while (num && pad_to != 0) {
				uint8_t nibble_low = (uint8_t)(num & 0xF);
				char hex_low = NibbleToHex(nibble_low);

				uint8_t nibble_high = (uint8_t)((num >> 4) & 0xF);
				char hex_high = NibbleToHex(nibble_high);

				ret[original_pad - (std::size_t)pad_to] = hex_high;
				pad_to--;

				if (pad_to) {
					ret[original_pad - (std::size_t)pad_to] = hex_low;
					pad_to--;
				}

				num >>= 8;
			}
		}
		else {
			while (num && pad_to != 0) {
				uint8_t nibble = (uint8_t)(num & 0xF);
				char hex = NibbleToHex(nibble);

				ret[(std::size_t)pad_to - 1] = hex;

				pad_to--;
				num >>= 4;
			}
		}

		return ret;
	}

	uint32_t Server::GetRegValueFromIndex(uint8_t reg_index) {
		auto& cpu_struct = m_sys->GetCPU();

		if (reg_index <= 31)
			return cpu_struct.GetRegs().array[reg_index];

		switch (reg_index)
		{
		case 32:
			return cpu_struct.GetCOP0().registers.sr.reg;
		case 33:
			return cpu_struct.GetLO();
		case 34:
			return cpu_struct.GetHI();
		case 35:
			return cpu_struct.GetCOP0().registers.badvaddr;
		case 36:
			return cpu_struct.GetCOP0().registers.cause.reg;
		case 37:
			return cpu_struct.GetPc();
		default: //Non-existent FP registers
			break;
		}

		return 0x0;
	}

	void Server::SetRegValueFromIndex(uint8_t reg_index, uint32_t value) {
		auto& cpu_struct = m_sys->GetCPU();

		if (reg_index <= 31) {
			cpu_struct.GetRegs().array[reg_index] = value;
			return;
		}

		switch (reg_index)
		{
		case 32:
			cpu_struct.GetCOP0().registers.sr.reg = value;
			break;
		case 33:
			cpu_struct.GetLO() = value;
			break;
		case 34:
			cpu_struct.GetHI() = value;
			break;
		case 35:
			cpu_struct.GetCOP0().registers.badvaddr = value;
			break;
		case 36:
			cpu_struct.GetCOP0().registers.cause.reg = value;
			break;
		case 37:
			cpu_struct.GetPc() = value;
			break;
		default: //Non-existent FP registers
			break;
		}
	}


	/// ////////////////////////////////////


	void Server::HandleHg(std::string& data) {
		SendPayload("OK");
	}

	void Server::HandleQAttached(std::string& data) {
		SendEmpty();
	}

	void Server::HandleQuestionMark(std::string& data) {
		SendPayload("S05"); //Always respond signal 5 (SIGTRAP)
	}

	void Server::HandleHc(std::string& data) {
		SendPayload("OK");
	}

	void Server::HandleQC(std::string& data) {
		SendEmpty();
	}

	/*
	The psx does not have a concept of threads, 
	unless you consider the BIOS implementation
	as "threads"...
	*/

	void Server::HandleQfThreadInfo(std::string& data) {
		SendPayload("l");
	}

	void Server::HandleQsThreadInfo(std::string& data) {
		SendPayload("l");
	}

	void Server::HandleSmallG(std::string& data) {
		//Send registers packet
		std::ostringstream out{};

		//Send all 73 regs (which means that a lot of them are dummy values)

		for (uint32_t reg_index = 0; reg_index <= NUM_STUB_REGISTERS; reg_index++)
			out << UintToHexString( GetRegValueFromIndex(reg_index), 8, true);

		std::string res = out.str();

		SendPayload(res);
	}

	void Server::HandleBigG(std::string& data) {
		uint32_t reg_index = 0;

		//Simply writes all registers
		//in one go

		while (data.length() >= 8) {
			auto hex_num = std::string_view(data.begin(), data.begin() + 8);

			auto res = HexStringToUint(hex_num, true);

			if (!res) {
				SendPayload("E00");
				return;
			}
			else {
				SetRegValueFromIndex(reg_index, res.value());
			}

			data.erase(0, 8);

			reg_index += 1;
		}

		SendPayload("OK");
	}

	void Server::HandleDetach(std::string& data) {
		SendPayload("OK");
		m_open = false;
	}

	void Server::HandleP(std::string& data) {
		//Format P[hex reg index]=[hex value]
		auto reg_index_end = data.find_first_of('=');

		if (reg_index_end == std::string::npos || (data.begin() + reg_index_end + 1) == data.end()) {
			SendPayload("E00");
			return;
		}

		auto reg_index_str = std::string_view(data.begin(), data.begin() + reg_index_end);

		auto reg_index = HexStringToUint(reg_index_str, false);

		if (!reg_index) {
			SendPayload("E00");
			return;
		}

		auto reg_value_str = std::string_view(data.begin() + reg_index_end + 1, data.end());
		auto reg_value = HexStringToUint(reg_value_str, true);

		if (!reg_value) {
			SendPayload("E00");
			return;
		}

		SetRegValueFromIndex(reg_index.value(), reg_value.value());

		SendPayload("OK");
	}

	void Server::HandleM(std::string& data) {
		auto colon_pos = data.find_first_of(',');
		auto addr_str = data.substr(0, colon_pos);
		auto size_str = data.substr(colon_pos + 1);

		auto addr = HexStringToUint(addr_str, false);

		if (!addr.has_value()) {
			SendPayload("E00");
			return;
		}

		auto size = HexStringToUint(size_str, false);

		if (!size.has_value()) {
			SendPayload("E00");
			return;
		}
		
		auto effective_addr = addr.value();
		auto effective_sz = size.value();

		std::string out = "";

		out.reserve((std::size_t)effective_sz * 2);

		while (effective_sz) {
			psx::u8 value = m_sys->GetStatus().sysbus->Read<u8, false>(effective_addr);
			auto hex_str = UintToHexString(value, 2, false);

			out.append(hex_str);

			effective_sz--;
			effective_addr++;
		}

		SendPayload(out);
	}

	Server::~Server() {
		Shutdown();

		if (m_recv_buffer)
			delete[] m_recv_buffer;
	}
}