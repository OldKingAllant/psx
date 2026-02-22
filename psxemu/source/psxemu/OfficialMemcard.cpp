#include <psxemu/include/psxemu/OfficialMemcard.hpp>
#include <common/Errors.hpp>

#include <fmt/format.h>

#include <psxemu/include/psxemu/Logger.hpp>
#include <psxemu/include/psxemu/LoggerMacros.hpp>

#include <filesystem>
#include <fstream>

namespace psx {
	OfficialMemcard::OfficialMemcard() :
		m_path{std::nullopt},
		m_state{CurrState::IDLE},
		m_flag{FlagByte::NONE},
		m_rd_status{ReadStatus::ID},
		m_wr_status{WriteStatus::ID},
		m_cmd_step{0}, m_cmd_max_steps{0},
		m_selected_sector{0}, m_is_sector_valid{false}, m_is_checksum_valid{false},
		m_mc_buffer{}, m_temp_sector{}, m_update_seq_number{}
	{
		auto temp_buf = std::make_unique<char[]>(SECTOR_SIZE);
		m_temp_sector.swap(temp_buf);
	}

	u8 OfficialMemcard::Send(u8 value) { 
		switch (m_state)
		{
		case psx::OfficialMemcard::CurrState::IDLE:
			return IDLE_ProcessByte(value);
			break;
		case psx::OfficialMemcard::CurrState::GETID:
			return GETID_ProcessByte(value);
			break;
		case psx::OfficialMemcard::CurrState::READ:
			return READ_ProcessByte(value);
			break;
		case psx::OfficialMemcard::CurrState::WRITE:
			return WRITE_ProcessByte(value);
			break;
		default:
			break;
		}

		return 0xFF;
	}

	bool OfficialMemcard::Ack() { 
		return true; 
	}

	void OfficialMemcard::Reset() {
		m_state = CurrState::IDLE;
		m_rd_status = ReadStatus::ID;
		m_wr_status = WriteStatus::ID;
		m_cmd_step = 0;
		m_selected_sector = 0;
		m_is_sector_valid = false;
		m_is_checksum_valid = false;
	}

	bool OfficialMemcard::LoadFile(std::string const& path) { 
		if (!std::filesystem::exists(path))
			return false;

		if (std::filesystem::file_size(path) != uintmax_t(MEMCARD_SIZE))
			return false;

		std::ifstream file{ path, std::ios::binary };

		if (!file.is_open())
			return false;

		auto temp_buf = std::make_unique<char[]>(MEMCARD_SIZE);
		m_mc_buffer.swap(temp_buf);
		file.read(m_mc_buffer.get(), MEMCARD_SIZE);

		m_path = path;

		return true; 
	}

	std::optional<std::vector<u8>> OfficialMemcard::ReadFrame(u32 frame_num) const {
		if (frame_num > MAX_SECTOR) {
			return std::nullopt;
		}

		std::vector<u8> data{};
		data.resize(SECTOR_SIZE);

		std::copy_n(m_mc_buffer.get() + (frame_num * SECTOR_SIZE), SECTOR_SIZE,
			data.data());

		return data;
	}

	bool OfficialMemcard::WriteFrame(u32 frame_num, std::vector<u8> const& data) {
		if (data.size() != SECTOR_SIZE) {
			return false;
		}
		if (frame_num > MAX_SECTOR) {
			return false;
		}

		std::copy_n(data.data(), SECTOR_SIZE, m_mc_buffer.get() + (frame_num * SECTOR_SIZE));
		m_update_seq_number += 1;
		
		return true;
	}

	OfficialMemcard::~OfficialMemcard() {}

	u8 OfficialMemcard::ComputeSectorChecksum(u32 sector_num) {
		u8 checksum{u8(sector_num >> 8)};
		checksum ^= u8(sector_num);

		u32 start = sector_num * SECTOR_SIZE;
		u32 end = start + SECTOR_SIZE;

		for (u32 curr_byte = start; curr_byte < end; curr_byte++) {
			checksum ^= m_mc_buffer.get()[curr_byte];
		}

		return checksum;
	}

	u8 OfficialMemcard::ComputeTempSectorChecksum(u32 sector_num) {
		u8 checksum{ u8(sector_num >> 8) };
		checksum ^= u8(sector_num);

		for (u32 curr_byte = 0; curr_byte < SECTOR_SIZE; curr_byte++) {
			checksum ^= m_temp_sector.get()[curr_byte];
		}

		return checksum;
	}

	u8 OfficialMemcard::IDLE_ProcessByte(u8 value) {
		Command cmd{ value };

		switch (cmd)
		{
		case psx::OfficialMemcard::Command::GETID:
			m_state = CurrState::GETID;
			break;
		case psx::OfficialMemcard::Command::READ:
			m_rd_status = ReadStatus::ID;
			m_state = CurrState::READ;
			break;
		case psx::OfficialMemcard::Command::WRITE:
			m_wr_status = WriteStatus::ID;
			m_state = CurrState::WRITE;
			break;
		default:
			break;
		}

		m_cmd_step = 0;
		m_cmd_max_steps = 2;

		return u8(m_flag);
	}

	u8 OfficialMemcard::READ_ProcessByte(u8 value) {
		u8 response{ 0xFF };

		switch (m_rd_status)
		{
		case psx::OfficialMemcard::ReadStatus::ID:
			response = CARD_ID[m_cmd_step];
			break;
		case psx::OfficialMemcard::ReadStatus::RECV_SECTOR:
			response = 0x00;
			m_selected_sector = u16((m_selected_sector << 8) | u32(value));
			m_is_sector_valid = m_selected_sector <= MAX_SECTOR;
			break;
		case psx::OfficialMemcard::ReadStatus::SEND_ACK:
			response = CMD_ACK[m_cmd_step];
			break;
		case psx::OfficialMemcard::ReadStatus::SEND_CONFIRM:
			if (!m_is_sector_valid) {
				response = 0xFF;
			}
			else {
				if (m_cmd_step == 0)
					response = u8(m_selected_sector >> 8);
				else
					response = u8(m_selected_sector);
			}
			break;
		case psx::OfficialMemcard::ReadStatus::SEND_SECTOR:
		{
			u32 sec_offset = m_selected_sector * SECTOR_SIZE;
			u32 tot_offset = sec_offset + m_cmd_step;
			response = m_mc_buffer.get()[tot_offset];
		}
			break;
		case psx::OfficialMemcard::ReadStatus::SEND_CHECKSUM:
			response = ComputeSectorChecksum(m_selected_sector);
			break;
		case psx::OfficialMemcard::ReadStatus::SEND_END_BYTE:
			response = END_BYTE;
			break;
		default:
			break;
		}

		m_cmd_step++;

		if (m_cmd_step == m_cmd_max_steps) {
			m_cmd_step = 0;
			if (m_rd_status == ReadStatus::SEND_END_BYTE ||
				(m_rd_status == ReadStatus::SEND_CONFIRM && !m_is_sector_valid)) {
				//abort the transer if the selected sector was invalid
				//and we already sent confirmation back to the program
				m_state = CurrState::IDLE;
				m_selected_sector = 0;
			}
			else {
				m_rd_status = ReadStatus(u32(m_rd_status) + 1);
				m_cmd_max_steps = READ_MAX_STEPS[u32(m_rd_status)];
			}
		}

		return response;
	}

	u8 OfficialMemcard::WRITE_ProcessByte(u8 value) {
		u8 response{ 0xFF };

		switch (m_wr_status)
		{
		case psx::OfficialMemcard::WriteStatus::ID:
			response = CARD_ID[m_cmd_step];
			break;
		case psx::OfficialMemcard::WriteStatus::RECV_SECTOR:
			response = 0x00;
			m_selected_sector = u16((m_selected_sector << 8) | u32(value));
			m_is_sector_valid = m_selected_sector <= MAX_SECTOR;
			break;
		case psx::OfficialMemcard::WriteStatus::RECV_DATA:
		{
			response = 0x00;
			m_temp_sector.get()[m_cmd_step] = value;
		}
		break;
		case psx::OfficialMemcard::WriteStatus::RECV_CHECKSUM:
			response = 0x00;
			if (m_is_sector_valid) {
				u8 computed_checksum = ComputeTempSectorChecksum(m_selected_sector);
				m_is_checksum_valid = computed_checksum == value;
			}
			else {
				m_is_checksum_valid = false;
			}
			break;
		case psx::OfficialMemcard::WriteStatus::SEND_ACK:
			response = CMD_ACK[m_cmd_step];
			break;
		case psx::OfficialMemcard::WriteStatus::SEND_END_BYTE:
			if (!m_is_sector_valid)
				response = BAD_SECTOR;
			else if (!m_is_checksum_valid)
				response = BAD_CHECKSUM;
			else {
				response = END_BYTE;
				u32 offset = m_selected_sector * SECTOR_SIZE;
				std::copy_n(m_temp_sector.get(), SECTOR_SIZE, m_mc_buffer.get() + offset);
				m_update_seq_number += 1;
			}
			break;
		default:
			break;
		}

		m_cmd_step++;

		if (m_cmd_step == m_cmd_max_steps) {
			m_cmd_step = 0;
			if (m_wr_status == WriteStatus::SEND_END_BYTE) {
				m_state = CurrState::IDLE;
				m_selected_sector = 0;
				m_flag = FlagByte::DIR_READ;
			}
			else {
				m_wr_status = WriteStatus(u32(m_wr_status) + 1);
				m_cmd_max_steps = WRITE_MAX_STEPS[u32(m_wr_status)];
			}
		}

		return response;
	}

	u8 OfficialMemcard::GETID_ProcessByte(u8 value) {
		LOG_ERROR("MEMCARD", "[MC] GETID");
		error::DebugBreak();
	}
}