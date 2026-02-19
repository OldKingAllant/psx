#include <psxemu/include/psxemu/formats/SystemCnf.hpp>

#include <stdexcept>
#include <string>
#include <string_view>
#include <bit>
#include <sstream>
#include <vector>
#include <algorithm>

namespace psx::kernel {
	SystemCnf::SystemCnf(std::span<u8> data) :
		m_entries{}
	{
		std::string complete_file{ std::bit_cast<char*>(data.data()), data.size() };
		std::istringstream file_data{};
		file_data.str(complete_file);
		std::string curr_line{};
		while (std::getline(file_data, curr_line)) {
			auto r_pos = curr_line.find('\r');
			if (r_pos != std::string::npos) {
				curr_line.erase(r_pos);
			}
			std::vector<std::string> line_parts{};
			std::string part{};
			std::istringstream line_stream{};
			line_stream.str(curr_line);
			while (std::getline(line_stream, part, ' '))
			{
				line_parts.push_back(part);
			}

			line_parts.erase(std::find_if(line_parts.begin(), line_parts.end(), [](std::string const& curr_part) {
				return curr_part.empty(); }), line_parts.end());

			if (line_parts.size() < 3 || line_parts[1] != "=") {
				continue;
			}

			if (line_parts[0] == "BOOT" && line_parts.size() == 4) {
				m_entries["ARGS"] = line_parts[3];
			}

			m_entries[line_parts[0]] = line_parts[2];
		}
	}

	std::optional<std::string> SystemCnf::GetBootFile() const {
		if (m_entries.contains("BOOT")) {
			return m_entries.find("BOOT")->second;
		}
		return std::nullopt;
	}

	std::optional<std::string> SystemCnf::GetBootArgs() const
	{
		if (m_entries.contains("ARGS")) {
			return m_entries.find("ARGS")->second;
		}
		return std::nullopt;
	}

	std::optional<u32> SystemCnf::GetTCB() const {
		if (!m_entries.contains("TCB")) {
			return std::nullopt;
		}
		try {
			u32 tcb = std::stoul(m_entries.find("TCB")->second, nullptr, 16);
			return tcb;
		}
		catch (std::invalid_argument&) {
			return std::nullopt;
		}
	}

	std::optional<u32> SystemCnf::GetEvent() const {
		if (!m_entries.contains("EVENT")) {
			return std::nullopt;
		}
		try {
			u32 event = std::stoul(m_entries.find("EVENT")->second, nullptr, 16);
			return event;
		}
		catch (std::invalid_argument&) {
			return std::nullopt;
		}
	}

	std::optional<u32> SystemCnf::GetStack() const {
		if (!m_entries.contains("STACK")) {
			return std::nullopt;
		}
		try {
			u32 stack = std::stoul(m_entries.find("STACK")->second, nullptr, 16);
			return stack;
		}
		catch (std::invalid_argument&) {
			return std::nullopt;
		}
	}
}