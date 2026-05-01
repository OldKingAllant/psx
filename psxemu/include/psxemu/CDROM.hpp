#pragma once

#include <psxemu/include/psxemu/CdLocation.hpp>

#include <filesystem>
#include <array>

#include <common/Defs.hpp>

namespace psx {
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

	struct SubModeByte {
		u8 eor       : 1;
		u8 video     : 1;
		u8 audio     : 1;
		u8 data      : 1;
		u8 trigger   : 1;
		u8 form2     : 1;
		u8 real_time : 1;
		u8 eof		 : 1;
	};

	struct CodingInfoByte {
		u8 is_stereo       : 2;
		u8 sample_rate     : 2;
		u8 bits_per_sample : 2;
		u8 emphasis        : 1;
		u8 reserved        : 1;
	};

	struct SectorMode2SubHeader {
		u8 file;
		u8 channel;
		SubModeByte submode;
		CodingInfoByte codinginfo;
	};

#pragma pack(push, 1)
	struct SectorMode2Form1 {
		char sync[0xC];
		SectorHeader header;
		SectorMode2SubHeader subheader;
		SectorMode2SubHeader subheader_copy;
		char data[LOGICAL_SECTOR_SIZE];
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

		virtual bool Init() = 0;

		virtual u64 GetTrackNumber(CdLocation loc) const = 0;
		virtual u64 GetTrackNumber(u64 lba) const = 0;

		virtual std::array<u8, FULL_SECTOR_SIZE> ReadSector(CdLocation loc) = 0;
		virtual std::array<u8, FULL_SECTOR_SIZE> ReadFullSector(CdLocation loc) = 0;

		virtual u64 GetFileSize(u64 track) const = 0;
		virtual CdLocation LogicalToPhysical(u64 lba) const = 0;

		virtual ~CDROM() {}

	protected :
		std::filesystem::path m_path;
	};
}