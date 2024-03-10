#pragma once

#include <common/Defs.hpp>
#include <common/Macros.hpp>

#include <stddef.h>
#include <bit>

namespace psx {
	/// <summary>
	/// Raw psx executable header
	/// </summary>
	struct psxexe_header {
#pragma pack(push, 1)
		//Should be "PS-X EXE"
		const char format_id[8];
		const char zerofilled[8];
		u32 start_pc;
		u32 start_gp;
		u32 dest_address;
		//N*0x800 without header
		u32 filesize; 
		u32 unused1;
		u32 unused2;
		u32 memfill_start;
		u32 memfill_size;
		//Set SP (R29) and FP (R30) to this value
		u32 sp_start;
		//Add this to sp_start
		u32 sp_offset;
		u8 unused3[20];
#pragma pack(pop)
	};

	class psxexe {
	public :
		psxexe(u8* data);
		~psxexe();

		FORCE_INLINE psxexe_header const* header() const {
			return std::bit_cast<psxexe_header const*>(m_data);
		}

		FORCE_INLINE u32 initial_sp() const {
			auto head = header();
			return head->sp_start ? head->sp_start + head->sp_offset : 0;
		}

	private :
		u8* m_data;
	};

	static_assert(OFFSETOFF(psxexe_header, unused3) == 0x38);
}