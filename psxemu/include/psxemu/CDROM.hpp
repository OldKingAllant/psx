#pragma once

#include <filesystem>
#include <array>

#include <common/Defs.hpp>

namespace psx {
	class CDROM {
	public :
		CDROM(std::filesystem::path const& rom_path) :
			m_path(rom_path) {}

		std::filesystem::path const& GetPath() const {
			return m_path;
		}

		static constexpr u64 SECTOR_SIZE = 0x800;
		static constexpr u64 FULL_SECTOR_SIZE = 0x930;

		virtual bool Init() = 0;

		virtual std::array<u8, SECTOR_SIZE> ReadSector(u64 amm, u64 ass, u64 asect) = 0;
		virtual std::array<u8, FULL_SECTOR_SIZE> ReadFullSector(u64 amm, u64 ass, u64 asect) = 0;

	protected :
		std::filesystem::path m_path;
	};
}