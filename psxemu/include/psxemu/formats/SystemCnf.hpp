#pragma once

#include <common/Defs.hpp>
#include <common/Macros.hpp>

#include <unordered_map>
#include <string>
#include <optional>
#include <span>

namespace psx::kernel {
	class SystemCnf {
	public :
		SystemCnf(std::span<u8> data);

		std::optional<std::string> GetBootFile() const;
		std::optional<std::string> GetBootArgs() const;
		std::optional<u32> GetTCB() const;
		std::optional<u32> GetEvent() const;
		std::optional<u32> GetStack() const;

	private :
		std::unordered_map<std::string, std::string> m_entries;
	};
}