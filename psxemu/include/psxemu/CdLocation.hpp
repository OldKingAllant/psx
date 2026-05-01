#pragma once

#include <common/Defs.hpp>

namespace psx {
	static constexpr u64 TRACKS_PER_DISK = 99;
	static constexpr u64 INDICES_PER_TRACK = 99;
	static constexpr u64 MINUTES_PER_DISK = 74;
	static constexpr u64 SECONDS_PER_MINUTE = 60;
	static constexpr u64 SECTORS_PER_SECOND = 75;

	static constexpr u64 LOGICAL_SECTOR_SIZE = 0x800;
	static constexpr u64 FULL_SECTOR_SIZE = 0x930;

	struct CdLocation {
		u64 mm;
		u64 ss;
		u64 sect;

		constexpr CdLocation(u64 mm, u64 ss, u64 sect) {
			this->mm = mm;
			this->ss = ss;
			this->sect = sect;
		}

		constexpr CdLocation() :
			mm{}, ss{}, sect{} {
		}

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

		static CdLocation sect_to_location(u64 sect) {
			CdLocation loc{};
			loc.sect = sect;
			if (loc.sect >= SECTORS_PER_SECOND) {
				loc.ss = loc.sect / SECTORS_PER_SECOND;
				loc.sect %= SECTORS_PER_SECOND;
			}

			if (loc.ss >= SECONDS_PER_MINUTE) {
				loc.mm = loc.ss / SECONDS_PER_MINUTE;
				loc.ss %= SECONDS_PER_MINUTE;
			}

			return loc;
		}

		CdLocation operator+(CdLocation other) const {
			return sect_to_location(to_sect() + other.to_sect());
		}

		CdLocation operator-(CdLocation other) const {
			if (to_sect() < other.to_sect()) {
				return CdLocation{};
			}
			return sect_to_location(to_sect() - other.to_sect());
		}

		bool operator>(CdLocation other) const {
			return to_lba() > other.to_lba();
		}

		bool operator<(CdLocation other) const {
			return to_lba() < other.to_lba();
		}

		bool operator>=(CdLocation other) const {
			return to_lba() >= other.to_lba();
		}

		bool operator<=(CdLocation other) const {
			return to_lba() <= other.to_lba();
		}

		u64 to_sect() const {
			return ((mm * SECONDS_PER_MINUTE)
				* SECTORS_PER_SECOND +
				(ss * SECTORS_PER_SECOND) +
				sect);
		}

		u64 to_lba() const {
			return ((mm * SECONDS_PER_MINUTE)
				* SECTORS_PER_SECOND +
				(ss * SECTORS_PER_SECOND) +
				sect) * FULL_SECTOR_SIZE;
		}

		static constexpr CdLocation from_lba(u64 lba) {
			CdLocation cdloc{};
			//ignore offset inside sector
			u64 abs_sect = lba / FULL_SECTOR_SIZE;
			cdloc.sect = abs_sect % SECTORS_PER_SECOND;
			u64 abs_second = abs_sect / SECTORS_PER_SECOND;
			cdloc.ss = abs_second % SECONDS_PER_MINUTE;
			u64 abs_minute = abs_second / SECONDS_PER_MINUTE;
			cdloc.mm = abs_minute;
			return cdloc;
		}
	};

	static u64 bcd_to_normal(u64 value) {
		u64 result{};
		u64 curr_power{ 1 };
		while (value) {
			result += (value & 0xF) * curr_power;
			curr_power *= 10;
			value >>= 4;
		}
		return result;
	}
}