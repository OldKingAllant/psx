#pragma once

#include <filesystem>
#include <array>

#include <common/Defs.hpp>

namespace psx {
	static constexpr u64 TRACKS_PER_DISK = 99;
	static constexpr u64 INDICES_PER_TRACK = 99;
	static constexpr u64 MINUTES_PER_DISK = 74;
	static constexpr u64 SECONDS_PER_MINUTE = 60;
	static constexpr u64 SECTORS_PER_SECOND = 75;

	struct CdLocation {
		u64 mm;
		u64 ss;
		u64 sect;

		CdLocation& operator++() {
			sect += 1;

			if (sect >= SECTORS_PER_SECOND) {
				ss += 1;
				sect %= SECTORS_PER_SECOND;
			}

			if (ss >= SECONDS_PER_MINUTE) {
				mm += 1;
				ss %= SECONDS_PER_MINUTE;
			}

			if (mm >= MINUTES_PER_DISK) {
				mm = 0;
			}

			return *this;
		}

		CdLocation& operator++(int) {
			sect += 1;

			if (sect >= SECTORS_PER_SECOND) {
				ss += 1;
				sect %= SECTORS_PER_SECOND;
			}

			if (ss >= SECONDS_PER_MINUTE) {
				mm += 1;
				ss %= SECONDS_PER_MINUTE;
			}

			if (mm >= MINUTES_PER_DISK) {
				mm = 0;
			}

			return *this;
		}

		u64 to_mode2_absolute() const {
			return ((mm * SECONDS_PER_MINUTE)
				* SECTORS_PER_SECOND + 
				(ss * SECTORS_PER_SECOND) + 
				sect) * 0x930;
		}
	};

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

		virtual std::array<u8, FULL_SECTOR_SIZE> ReadSector(u64 amm, u64 ass, u64 asect) = 0;
		virtual std::array<u8, FULL_SECTOR_SIZE> ReadFullSector(u64 amm, u64 ass, u64 asect) = 0;

	protected :
		std::filesystem::path m_path;
	};
}