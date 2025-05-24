#pragma once

#include <common/Defs.hpp>
#include <common/Macros.hpp>
#include <common/Queue.hpp>

#include "CDDriveStructs.hpp"
#include "CDROM.hpp"

#include <list>
#include <memory>
#include <filesystem>
#include <array>

class DebugView;

namespace psx {
	struct system_status;

	static constexpr u32 CDROM_REGS_BASE = 0x800;
	static constexpr u32 CDROM_REGS_END = 0x803;

	struct CdLocation {
		u64 mm;
		u64 ss;
		u64 sect;
	};

	class CDDrive {
	public :
		CDDrive(system_status* sys_status);

		/*
		Unfortunately, in this instance we
		need to explicitly differentiate
		between different sizes of read/writes,
		since all registers are accessed 
		only with a size of 1 byte
		(psx-nocash says that behaviour
		is unknown/undefined when accessing
		with 16/32 bits)
		*/


		u8 Read8(u32 address);
		u16 Read16(u32 address);
		u32 Read32(u32 address);

		void Write8(u32 address, u8 value);
		void Write16(u32 address, u16 value);
		void Write32(u32 address, u32 value);

		void WriteReg1(u8 value);
		void WriteReg2(u8 value);
		void WriteReg3(u8 value);

		u8 ReadReg1();
		u8 ReadReg2();
		u8 ReadReg3();

		friend void event_callback(void* userdata, u64 cycles_late);
		friend void read_callback(void* userdata, u64 cycles_late);

		void OpenLid();
		void CloseLid();
		bool InsertDisc(std::filesystem::path const& path);

	private :
		void HandlePendingCommand();
		void CommandExecute();

		//////////

		void CommandTest(u8 cmd);
		void Command_GetStat();
		void Command_GetID();
		void Command_Setmode();
		void Command_Stop();
		void Command_ReadTOC();
		void Command_SetLoc();
		void Command_SeekL();
		void Command_ReadN();

		///////////

		void Command_GetBiosBCD();

		//////////////

		bool ValidateParams(u32 num_params);

		void InterruptAckd();
		void RequestInterrupt(CdInterrupt interrupt);
		void PushResponse(CdInterrupt interrupt, std::initializer_list<u8> args, u64 delay);
		u8 PopResponseByte();
		void DeliverInterrupt(u64 cycles_late);
		void ScheduleInterrupt(u64 cycles);
		void ReadCallback(u64 cycles_late);

		std::string const& GetConsoleRegion() const;

		friend class DebugView;

	private :
		IndexRegister m_index_reg;
		u8 m_curr_cmd;
		u8 m_new_cmd;
		Queue<Response, 3> m_response_fifo;
		Queue<u8, 16> m_param_fifo;

		InterruptEnable m_int_enable;
		InterruptFlag m_int_flag;

		bool m_cmd_start_interrupt;
		bool m_want_data;

		XA_ADPCM_Volume m_volume;
		bool m_mute_adpcm;
		SoundCoding m_sound_coding;

		bool m_read_paused;
		bool m_motor_on;

		Mode m_mode;
		Stat m_stat;

		system_status* m_sys_status;
		bool m_idle;
		bool m_has_next_cmd;

		u64 m_event_id;

		bool m_keep_history;
		std::list<DriveCommand> m_history;

		std::unique_ptr<CDROM> m_cdrom;
		bool m_lid_open;

		CdLocation m_seek_loc;
		CdLocation m_unprocessed_seek_loc;
		bool m_has_unprocessed_seek;

		u64 m_read_event;
		bool m_has_pending_read;
		std::array<u8, CDROM::FULL_SECTOR_SIZE> m_curr_sector;
		std::array<u8, CDROM::FULL_SECTOR_SIZE> m_pending_sector;
		bool m_has_data_to_load;
		bool m_has_loaded_data;
	};
}