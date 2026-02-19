#pragma once

#include <unordered_map>
#include <string>
#include <span>

#include <common/Defs.hpp>

namespace psx::kernel {
	static constexpr u64 SCPH1001 = 1001;

	//1001
	static const auto KNOWN_BIOSES = std::unordered_map<std::string, u64>{
		{ std::string{"71af94d1e47a68c11e8fdb9f8368040601514a42a5a399cda48c7d3bff1e99d3"}, SCPH1001}
	};

	std::string CalcBiosSHA256(std::span<char> buf);
}