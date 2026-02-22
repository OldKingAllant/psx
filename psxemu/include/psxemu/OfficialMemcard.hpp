#pragma once

#include "AbstractMemcard.hpp"

#include <memory>
#include <vector>
#include <optional>

namespace psx {
	class OfficialMemcard : public AbstractMemcard {
	public :
		OfficialMemcard();

		u8 Send(u8 value) override;
		bool Ack() override;
		void Reset() override;
		bool LoadFile(std::string const& path) override;

		u32 GetUpdateSequenceNumber() const override {
			return m_update_seq_number;
		}

		std::optional<std::vector<u8>> ReadFrame(u32 frame_num) const override;
		bool WriteFrame(u32 frame_num, std::vector<u8> const& data) override;

		bool IsConnected() const override {
			return m_path.has_value();
		}

		std::optional<std::string> GetMcPath() const override {
			return m_path;
		}

		virtual std::string GetType() const override {
			return "OFFICIAL";
		}

		~OfficialMemcard() override;

		enum class CurrState {
			IDLE,
			GETID,
			READ,
			WRITE
		};

		enum class Command : u8 {
			GETID = 0x53,
			READ = 0x52,
			WRITE = 0x57
		};

		enum class FlagByte : u8 {
			NONE = 0x08,
			DIR_READ = 0x00
		};

		enum class ReadStatus {
			ID,
			RECV_SECTOR,
			SEND_ACK,
			SEND_CONFIRM,
			SEND_SECTOR,
			SEND_CHECKSUM,
			SEND_END_BYTE
		};

		enum class WriteStatus {
			ID,
			RECV_SECTOR,
			RECV_DATA,
			RECV_CHECKSUM,
			SEND_ACK,
			SEND_END_BYTE
		};

		static constexpr u8 CARD_ID[] = { 0x5A, 0x5D };
		static constexpr u8 CMD_ACK[] = { 0x5C, 0x5D };

		static constexpr u32 READ_MAX_STEPS[] = { 2, 2, 2, 2, 128, 1, 1 };
		static constexpr u32 WRITE_MAX_STEPS[] = { 2, 2, 128, 1, 2, 1 };

		static constexpr u32 MAX_SECTOR = 0x3FF;

		static constexpr u32 MEMCARD_SIZE = 128 * 1024;
		static constexpr u32 NUM_BLOCKS = 16;
		static constexpr u32 BLOCK_SIZE = MEMCARD_SIZE / NUM_BLOCKS;
		static constexpr u32 SECTOR_SIZE = MEMCARD_SIZE / (MAX_SECTOR + 1);
		static constexpr u32 SECTORS_PER_BLOCK = BLOCK_SIZE / SECTOR_SIZE;

		static constexpr u8 END_BYTE = u8('G');
		static constexpr u8 BAD_CHECKSUM = u8(0x4E);
		static constexpr u8 BAD_SECTOR = u8(0xFF);

	private :
		u8 IDLE_ProcessByte(u8 value);
		u8 READ_ProcessByte(u8 value);
		u8 WRITE_ProcessByte(u8 value);
		u8 GETID_ProcessByte(u8 value);

		u8 ComputeSectorChecksum(u32 sector_num);
		u8 ComputeTempSectorChecksum(u32 sector_num);

	private :
		std::optional<std::string> m_path;
		CurrState m_state;
		FlagByte m_flag;
		ReadStatus m_rd_status;
		WriteStatus m_wr_status;
		u32 m_cmd_step;
		u32 m_cmd_max_steps;

		u32 m_selected_sector;
		bool m_is_sector_valid;
		bool m_is_checksum_valid;

		std::unique_ptr<char[]> m_mc_buffer;
		std::unique_ptr<char[]> m_temp_sector;

		u32 m_update_seq_number;
	};
}