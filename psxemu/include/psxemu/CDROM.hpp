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

	static u64 bcd_to_normal(u64 value) {
		u64 result{};
		u64 curr_power{1};
		while (value) {
			result += (value & 0xF) * curr_power;
			curr_power *= 10;
			value >>= 4;
		}
		return result;
	}

	/*
	  000h 0Ch  Sync   (00h,FFh,FFh,FFh,FFh,FFh,FFh,FFh,FFh,FFh,FFh,00h)
	  00Ch 4    Header (Minute,Second,Sector,Mode=02h)
	  010h 4    Sub-Header (File, Channel, Submode AND DFh, Codinginfo)
	  014h 4    Copy of Sub-Header
	  018h 800h Data (2048 bytes)
	  818h 4    EDC (checksum across [010h..817h])
	  81Ch 114h ECC (error correction codes)
	*/
	struct SectorHeader {
		u8 mm;
		u8 ss;
		u8 sect;
		u8 mode;
	};

	struct SectorMode2SubHeader {
		u8 file;
		u8 channel;
		u8 submode;
		u8 codinginfo;
	};

#pragma pack(push, 1)
	struct SectorMode2Form1 {
		char sync[0xC];
		SectorHeader header;
		SectorMode2SubHeader subheader;
		SectorMode2SubHeader subheader_copy;
		char data[0x800];
		u32 edc;
		char ecc[0x114];
	};
#pragma pack(pop)

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

		virtual ~CDROM() {}

	protected :
		std::filesystem::path m_path;
	};
}