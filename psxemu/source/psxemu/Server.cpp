#include <psxemu/include/psxemu/Server.hpp>
#include <psxemu/include/psxemu/ServerAsyncCommands.hpp>

#include <Poco/Net/SocketStream.h>

#include <chrono>
#include <algorithm>
#include <sstream>

#include <psxemu/include/psxemu/System.hpp>
#include <psxemu/include/psxemu/SystemBus.hpp>

#include <psxemu/include/psxemu/Logger.hpp>
#include <psxemu/include/psxemu/LoggerMacros.hpp>

#include <common/SetThreadName.hpp>

namespace psx::gdbstub {
	Server::Server(int16_t port, System* sys) :
		m_socket{},
		m_conn{},
		m_address{},
		m_address_lock{},
		m_port{port},
		m_open{false},
		m_recv_buffer{nullptr},
		m_out{},
		m_recv_size{},
		m_cmd_handlers{},
		m_ext_cmd_handlers{},
		m_trace_handler{}, m_tracing{false}, 
		m_sys{sys},
		m_thread_state{} {
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
		m_cmd_handlers.insert(std::pair{ std::string("M"), &Server::HandleBigM });
		m_cmd_handlers.insert(std::pair{ std::string("vCont?"), &Server::HandleVContQuestionMark });
		m_cmd_handlers.insert(std::pair{ std::string("vCont"), &Server::HandleVCont });
		m_cmd_handlers.insert(std::pair{ std::string("Z1"), &Server::HandleZ1 });
		m_cmd_handlers.insert(std::pair{ std::string("z1"), &Server::HandleSmallZ1 });
		m_cmd_handlers.insert(std::pair{ std::string("ext_"), &Server::HandleExtensionPackets});

		InitExtHandlers();

		auto server_address = Poco::Net::SocketAddress("127.0.0.1", m_port);;
		m_socket.bind(server_address);
	}

	void Server::Start(std::stop_token token) {
		using namespace std::chrono_literals;

		m_socket.listen(1);

		Poco::Net::PollSet poll_set{};
		poll_set.add(m_socket, Poco::Net::Socket::SELECT_READ);

		bool accepted = false;
		while (!token.stop_requested()) {
			using namespace std::chrono_literals;
			auto changed_sockets = poll_set.poll(Poco::Timespan(20ms));
			if (changed_sockets.contains(m_socket)) {
				m_conn = m_socket.acceptConnection();
				accepted = true;
				break;
			}
		}

		if (!accepted) {
			return;
		}

		m_conn.setReceiveTimeout(Poco::Timespan(
			TIMEOUT
		));

		if (!m_recv_buffer) {
			m_recv_buffer = std::make_unique<char[]>(BUFFER_SIZE);
		}

		std::scoped_lock lk{ m_address_lock };
		m_address = m_conn.peerAddress();
		m_open = true;
	}

	void Server::Shutdown() {
		if (m_open.load()) {
			m_conn.close();
			m_open.store(false);
			std::scoped_lock lk{ m_address_lock };
			m_address = {};
		}
	}

	bool Server::HandlePackets() {
		//The packet format is specified at https://sourceware.org/gdb/current/onlinedocs/gdb.html/Overview.html#Overview
		try {
			auto stop_token = m_thread_state.server_thread.get_stop_token();

			Poco::Net::PollSet poll_set{};
			poll_set.add(m_conn, Poco::Net::Socket::SELECT_READ | Poco::Net::Socket::SELECT_ERROR);

			using namespace std::chrono_literals;
			auto changed_sockets = poll_set.poll(Poco::Timespan(TIMEOUT));

			if (stop_token.stop_requested()) {
				return false;
			}

			if (!changed_sockets.contains(m_conn)) {
				return true;
			}

			if (changed_sockets[m_conn] & Poco::Net::Socket::SELECT_ERROR) {
				return false;
			}

			auto effective_len = m_conn.receiveBytes(m_recv_buffer.get(), BUFFER_SIZE);

			if (effective_len == 0)
				return true;

			m_recv_size = effective_len;

			std::string data{ m_recv_buffer.get(), (std::size_t)effective_len};

			if (m_tracing && m_trace_handler)
				m_trace_handler(data, false);

			//The connection might be closed by a command
			//and a single packet might contain more
			//than one command
			while (!stop_token.stop_requested() && data.length() && m_open.load()) {
				if (data[0] == '+' || data[0] == '\0') {
					data.erase(0, 1);
				}
				else if (data[0] == '-') {
					//SendPayload(m_out);
					data.erase(0, 1);
				}
				else if (data[0] == (char)0x03) {
					PushServerToEmuCommand<StopEmuCommand>();
					auto response = AwaitEmuResponse<StopEmuResponse>(stop_token);
					if (!response) {
						break;
					}
					SendPayload("S05");
					return true;
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
					auto checksum_str = std::string(data.begin() + end + 1,
						data.begin() + end + 3);

					//Data inside the leading $ and trailing #
					auto payload = std::string_view(data.begin() + start, data.begin() + end);
					auto computed_checksum = ComputeChecksum(payload); //Our checksum

					uint32_t converted_checksum{};
					try {
						converted_checksum = std::stoul(checksum_str, nullptr, 16);
					}
					catch (std::exception const& e) {
						LOG_ERROR("GDBSTUB", "[GDBSTUB] Invalid packet checksum: {}, error: {}",
							checksum_str, e.what());
						return true;
					}

					if (computed_checksum != converted_checksum) {
						SendNAck(); //Request retransmission
					}
					else {
						SendAck();

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
		catch (Poco::TimeoutException const&) {
			return m_open;
		}
		catch (std::exception const&) {
			return false;
		}

		return m_open;
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

	void Server::SendAck() {
		constexpr const char ACK[] = "+";

		m_conn.sendBytes(ACK, (int)sizeof(ACK));
	}

	void Server::SendPayload(std::string_view payload) {
		//Directly encode payload
		m_out = payload;

		std::string out_str = "";

		std::ostringstream stream{};

		//stream << "+";
		stream << "$";
		stream << payload;
		stream << "#";

		uint8_t checksum = ComputeChecksum(payload);

		stream << fmt::format("{:02x}", checksum);
		stream << "\0"; //Add trailing \0 just to be sure

		out_str = stream.str();

		if (m_tracing && m_trace_handler)
			m_trace_handler(out_str, true);

		m_conn.sendBytes(out_str.data(), (int)out_str.size());
	}

	void Server::HandleUnkownCommand(std::string const& name, std::string const& data) {
		LOG_WARN("GDBSTUB", "[GDBSTUB] Invalid/Unknown command encountered : {}", name);
		
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
		if (reg_index == 0)
			return;

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
		PushServerToEmuCommand<GetRegistersCommand>();
		auto response = AwaitEmuResponse<GetRegistersResponse>(m_thread_state.server_thread.get_stop_token());
		if (!response) {
			return;
		}

		//Send registers packet
		std::ostringstream out{};
		//Send all 73 regs (which means that a lot of them are dummy values)

		for (uint32_t reg_index = 0; reg_index < NUM_STUB_REGISTERS; reg_index++) {
			out << fmt::format("{:08x}", std::byteswap(response->regs[reg_index]));
		}
		//out << UintToHexString( response->regs[reg_index], 8, true);

		std::string res = out.str();

		SendPayload(res);
	}

	void Server::HandleBigG(std::string& data) {
		uint32_t reg_index = 0;

		//Simply writes all registers
		//in one go

		std::array<uint32_t, NUM_STUB_REGISTERS> regs{};

		while (data.length() >= 8) {
			auto hex_num = std::string(data.begin(), data.begin() + 8);
			uint32_t new_value{};

			try {
				new_value = std::stoul(hex_num, nullptr, 16);
			}
			catch (std::invalid_argument const& e) {
				LOG_ERROR("GDBSTUB", "[GDBSTUB] While handling G: {}", e.what());
				SendPayload("E00");
				return;
			}

			regs[reg_index] = std::byteswap(new_value);
			data.erase(0, 8);
			reg_index += 1;
		}

		PushServerToEmuCommand<SetRegistersCommand>(regs);
		SendPayload("OK");
	}

	void Server::HandleDetach(std::string& data) {
		SendPayload("OK");
		m_open.store(false);
	}

	void Server::HandleP(std::string& data) {
		//Format P[hex reg index]=[hex value]
		auto reg_index_end = data.find_first_of('=');

		if (reg_index_end == std::string::npos || (data.begin() + reg_index_end + 1) == data.end()) {
			SendPayload("E00");
			return;
		}

		auto reg_index_str = std::string(data.begin(), data.begin() + reg_index_end);
		uint32_t reg_index{};

		try {
			reg_index = std::stoul(reg_index_str, nullptr, 16);
		}
		catch (std::invalid_argument const& e) {
			LOG_ERROR("GDBSTUB", "[GDBSTUB] While handling P, invalid register: {}", e.what());
			SendPayload("E00");
			return;
		}

		auto reg_value_str = std::string(data.begin() + reg_index_end + 1, data.end());
		uint32_t reg_value{};

		try {
			reg_value = std::stoul(reg_value_str, nullptr, 16);
		}
		catch (std::invalid_argument const& e) {
			LOG_ERROR("GDBSTUB", "[GDBSTUB] While handling P, invalid value: {}", e.what());
			SendPayload("E00");
			return;
		}

		PushServerToEmuCommand<SetSingleRegisterCommand>(reg_index, reg_value);
		SendPayload("OK");
	}

	void Server::HandleM(std::string& data) {
		auto colon_pos = data.find_first_of(',');
		auto addr_str = data.substr(0, colon_pos);
		auto size_str = data.substr(colon_pos + 1);

		uint32_t addr{};
		uint32_t size{};

		try {
			addr = std::stoul(addr_str, nullptr, 16);
			size = std::stoul(size_str, nullptr, 16);
		}
		catch (std::invalid_argument const& e) {
			LOG_ERROR("GDBSTUB", "[GDBSTUB] While handling m, invalid value: {}", e.what());
			SendPayload("E00");
			return;
		}

		PushServerToEmuCommand<ReadMemoryCommand>(addr, size);
		auto response = AwaitEmuResponse<ReadMemoryResponse>(m_thread_state.server_thread.get_stop_token());
		if (!response) {
			return;
		}

		std::string out = "";
		out.reserve((std::size_t)size * 2);

		for (auto value : response->values) {
			out.append(fmt::format("{:02x}", value));
		}

		SendPayload(out);
	}

	void Server::HandleBigM(std::string& data) {
		auto colon_pos = data.find_first_of(',');
		auto value_pos = data.find_first_of(':');

		if (colon_pos == std::string::npos || value_pos == std::string::npos ||
			colon_pos > value_pos) {
			SendPayload("E00");
			return;
		}

		auto addr_str = data.substr(0, colon_pos);
		auto size_str = data.substr(colon_pos + 1, value_pos - colon_pos - 1);

		uint32_t addr{};
		uint32_t size{};

		try {
			addr = std::stoul(addr_str, nullptr, 16);
			size = std::stoul(size_str, nullptr, 16);
		}
		catch (std::exception const& e) {
			LOG_ERROR("GDBSTUB", "[GDBSTUB] While handling M, invalid value: {}", e.what());
			SendPayload("E00");
			return;
		}

		auto bytes_to_write_str = data.substr(value_pos + 1);

		std::vector<uint8_t> bytes{};
		bytes.reserve(size);

		size_t size_copy{ size };
		while (size_copy) {
			auto curr_byte = std::string(
				bytes_to_write_str.begin(),
				bytes_to_write_str.begin() + 2);

			uint8_t value = {};
			try {
				value = (uint8_t)std::stoul(curr_byte, nullptr, 16);
			}
			catch (std::exception const& e) {
				LOG_ERROR("GDBSTUB", "[GDBSTUB] While handling M, invalid value: {}", e.what());
				SendPayload("E00");
				return;
			}

			bytes.push_back(value);

			bytes_to_write_str.erase(0, 2);
			size_copy--;
		}

		PushServerToEmuCommand<WriteMemoryCommand>(addr, std::move(bytes));
		SendPayload("OK");
	}

	void Server::HandleVContQuestionMark(std::string& data) {
		SendPayload("OK");
	}

	void Server::HandleVCont(std::string& data) {
		if (data[0] == ';')
			data.erase(0, 1);

		auto division_pos = data.find_first_of(';');

		if (division_pos != std::string::npos && division_pos != data.size() - 1) {
			auto second_part = data.substr(division_pos + 1);
			auto first_part = data.substr(0, division_pos);

			char cmd = first_part[0];

			if (first_part.find_first_of(':') == std::string::npos) {
				if (second_part.find_first_of(':') == std::string::npos) {
					SendPayload("E00");
					return;
				}
				else {
					cmd = second_part[0];
				}
			}

			if (cmd == 's') {
				PushServerToEmuCommand<RunForNInstructionsCommand>(1);
				auto response = AwaitEmuResponse<RunInstructionsResponse>(m_thread_state.server_thread.get_stop_token());
				if (!response) {
					return;
				}
				SendPayload("S05");
			}
			else if (cmd == 'c') {
				PushServerToEmuCommand<ContinueCommand>();
			}
			else {
				SendPayload("E00");
			}
		}
		else {
			if (data[0] == 's') {
				PushServerToEmuCommand<RunForNInstructionsCommand>(1);
				auto response = AwaitEmuResponse<RunInstructionsResponse>(m_thread_state.server_thread.get_stop_token());
				if (!response) {
					return;
				}
				SendPayload("S05");
			}
			else if (data[0] == 'c') {
				PushServerToEmuCommand<ContinueCommand>();
			}
			else {
				SendPayload("E00");
			}
		}
	}

	void Server::HandleZ1(std::string& data) {
		if (data[0] == ',')
			data.erase(0, 1);

		auto colon_pos = data.find_first_of(',');

		if (colon_pos == std::string::npos) {
			SendPayload("E00");
			return;
		}

		auto address_str = data.substr(0, colon_pos);

		uint32_t address{};
		try {
			address = std::stoul(address_str, nullptr, 16);
		}
		catch (std::exception const& e) {
			LOG_ERROR("GDBSTUB", "[GDBSTUB] While handling Z1, invalid value: {}", e.what());
			SendPayload("E00");
			return;
		}

		PushServerToEmuCommand<AddHardwareBreakpointCommand>(address);
		SendPayload("OK");
	}

	void Server::HandleSmallZ1(std::string& data) {
		if (data[0] == ',')
			data.erase(0, 1);

		auto colon_pos = data.find_first_of(',');

		if (colon_pos == std::string::npos) {
			SendPayload("E00");
			return;
		}

		auto address_str = data.substr(0, colon_pos);

		uint32_t address{};
		try {
			address = std::stoul(address_str, nullptr, 16);
		}
		catch (std::exception const& e) {
			LOG_ERROR("GDBSTUB", "[GDBSTUB] While handling z1, invalid value: {}", e.what());
			SendPayload("E00");
			return;
		}

		PushServerToEmuCommand<RemoveHardwareBreakpointCommand>(address);
		SendPayload("OK");
	}

	void Server::BreakTriggered() {
		SendPayload("S05");
	}

	void Server::SignalBreakpoint() {
		PushEmuToServerCommand<BreakTriggeredResponse>();
	}

	Server::~Server() {
		StopThread();
		m_socket.close();
	}

	void Server::HandleExtensionPackets(std::string& data) {
		auto arg_start = data.find_first_of(':');

		if (arg_start == 0) {
			SendPayload("WHAT");
			return;
		}

		std::string command{};
		std::string args{};

		if (arg_start == std::string::npos) {
			command = data;
			args = "";
		}
		else {
			command = data.substr(0, arg_start);
			args = data.substr(arg_start + 1);
		}

		if (!m_ext_cmd_handlers.contains(command)) {
			SendPayload("Invalid EXT command");
			return;
		}

		auto const& handler = m_ext_cmd_handlers[command];
		std::invoke(handler, this, args);
	}

	void Server::StartThread() {
		if (m_thread_state.is_running.load()) {
			return;
		}
		m_thread_state.server_to_emu.clear();
		m_thread_state.emu_to_server.clear();
		m_thread_state.server_thread = std::jthread([this](std::stop_token token) {
			ThreadMain(token);
		});
		m_thread_state.is_running.store(true);
	}

	void Server::StopThread() {
		if (!m_thread_state.is_running.load()) {
			return;
		}
		m_thread_state.server_thread.request_stop();
		m_thread_state.server_thread.join();
		m_thread_state.server_to_emu.clear();
		m_thread_state.emu_to_server.clear();
		m_thread_state.is_running.store(false);
	}

	void Server::ThreadMain(std::stop_token token) {
		SetThreadName("GDBServer");

		while (!token.stop_requested()) { //Even if client closed the connection
										  //stay in the loop
			this->Start(token); //Wait for client connection

			PushServerToEmuCommand<StopEmuCommand>(); //Stop the emulator if it is running
			auto response = AwaitEmuResponse<StopEmuResponse>(token);
			if (!response) {
				break;
			}

			while (!token.stop_requested() && m_open.load()) {
				if (!HandlePackets()) break; //Client closed the connection
				HandleEmuCommands();
			}
		}

		Shutdown(); //Exit requested from the application, stop the thread
	}
}