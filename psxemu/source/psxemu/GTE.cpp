#include <psxemu/include/psxemu/GTE.hpp>
#include <psxemu/include/psxemu/SystemStatus.hpp>
#include <psxemu/include/psxemu/SystemBus.hpp>

#include <common/Errors.hpp>

#include <psxemu/include/psxemu/Logger.hpp>
#include <psxemu/include/psxemu/LoggerMacros.hpp>

#include <thirdparty/magic_enum/include/magic_enum/magic_enum.hpp>

#include <array>

namespace psx::cpu {
	GTE::GTE(system_status* sys_status) :
		m_regs{},
		m_sys_status{sys_status},
		m_cmd_interlock{}
	{}

	u32 GTE::ReadData(u8 reg) {
		InterlockCommand();

		auto reg_id = GTE_Regs::DATA_REGS_BASE + reg;

		switch (reg_id)
		{
		case 1:
			return u32(sign_extend<i32, 15>(m_regs.v0.vec[2]));
		case 3:
			return u32(sign_extend<i32, 15>(m_regs.v1.vec[2]));
		case 5:
			return u32(sign_extend<i32, 15>(m_regs.v2.vec[2]));
		case 7:
			return u32(m_regs.avg_z.vec[0]);
		case 8:
			return u32(sign_extend<i32, 15>(m_regs.ir0.vec[0]));
		case 9:
			return u32(sign_extend<i32, 15>(m_regs.ir1.vec[0]));
		case 10:
			return u32(sign_extend<i32, 15>(m_regs.ir2.vec[0]));
		case 11:
			return u32(sign_extend<i32, 15>(m_regs.ir3.vec[0]));
		case 15:
			return u32(u16(m_regs.sxy2.vec[0])) | (u32(m_regs.sxy2.vec[1]) << 16);
		default:
			break;
		}

		return m_regs.array[reg_id];
	}

	u32 GTE::ReadControl(u8 reg) {
		InterlockCommand();

		auto reg_id = GTE_Regs::CONTROL_REGS_BASE + reg;

		switch (reg_id)
		{
		case 36:
			return m_regs.rotation_matrix.read_last();
		case 44:
			return m_regs.light_matrix.read_last();
		case 52:
			return m_regs.light_color_mat.read_last();
		case 58:
			return u32(sign_extend<i32, 15>(m_regs.proj_plane_dist.vec[0]));
		case 59:
			return u32(sign_extend<i32, 15>(m_regs.dqa));
		case 61:
			return u32(sign_extend<i32, 15>(m_regs.avg_z_scale_0.vec[0]));
		case 62:
			return u32(sign_extend<i32, 15>(m_regs.avg_z_scale_1.vec[0]));
		case 63:
		{
			auto all_bits = m_regs.flags.raw & ~(1 << 31);
			all_bits &= (0xFF << 23) | (0x3F << 13);
			if (all_bits != 0) {
				m_regs.flags.raw |= (1 << 31);
			}
			else {
				m_regs.flags.raw &= ~(1 << 31);
			}
			return m_regs.flags.raw;
		}
		default:
			break;
		}
		return m_regs.array[reg_id];
	}

	void GTE::WriteData(u8 reg, u32 val) {
		auto reg_id = GTE_Regs::DATA_REGS_BASE + reg;

		auto clamp = [](auto value, auto min, auto max) {
			if (value < min)
				return min;
			if (value > max)
				return max;
			return value;
		};

		switch (reg_id)
		{
		case 9:
		{
			auto r = clamp(i16(val) / 0x80, 0x0, 0x1F);
			RGB_555 color = u16(m_regs.irgb.vec[1]);
			color.set_red(r);
			m_regs.irgb.vec[0] = u16(color);
			m_regs.irgb.vec[1] = u16(color);
		}
		break;
		case 10:
		{
			auto g = clamp(i16(val) / 0x80, 0x0, 0x1F);
			RGB_555 color = u16(m_regs.irgb.vec[1]);
			color.set_green(g);
			m_regs.irgb.vec[0] = u16(color);
			m_regs.irgb.vec[1] = u16(color);
		}
		break;
		case 11:
		{
			auto b = clamp(i16(val) / 0x80, 0x0, 0x1F);
			RGB_555 color = u16(m_regs.irgb.vec[1]);
			color.set_blue(b);
			m_regs.irgb.vec[0] = u16(color);
			m_regs.irgb.vec[1] = u16(color);
		}
		break;
		case 15: 
		{
			GTE_Vec2<i16> coords{};
			coords.vec[0] = i16(val >> 00);
			coords.vec[1] = i16(val >> 16);
			PushScreenFifo(coords);
		}
		break;
		case 28:
		{
			RGB_555 color = u16(val);
			val &= 0x7FFF;
			m_regs.ir3.vec[0] = i16(color.get_blue())  * 0x80;
			m_regs.ir2.vec[0] = i16(color.get_green()) * 0x80;
			m_regs.ir1.vec[0] = i16(color.get_red())   * 0x80;
			m_regs.irgb.vec[1] = u16(val);
		}
		break;
		case 30: 
		{
			auto signed_val = i32(val);
			u32 count{};
			if (signed_val >= 0) {
				count = std::countl_zero(val);
			}
			else {
				count = std::countl_one(val);
			}
			m_regs.lzc.vec[1] = count;
		}
		break;
		case 7:
		case 16:
		case 17:
		case 18:
		case 19:
			val &= 0xFFFF;
			break;
		case 29: //Read-only
		case 31:
			return;
		default:
			break;
		}

		m_regs.array[reg_id] = val;
	}

	void GTE::WriteControl(u8 reg, u32 val) {
		auto reg_id = GTE_Regs::CONTROL_REGS_BASE + reg;

		switch (reg_id)
		{
		case 63:
			val &= ~(0xFFF);
			break;
		default:
			break;
		}

		m_regs.array[reg_id] = val;
	}

	static constexpr auto generate_opcode_cycles_table() {
		std::array<u64, 0x40> OPCODE_CYCLES = {};

		OPCODE_CYCLES[0x01] = 15;
		OPCODE_CYCLES[0x06] = 8;
		OPCODE_CYCLES[0x0C] = 6;
		OPCODE_CYCLES[0x10] = 8;
		OPCODE_CYCLES[0x11] = 8;
		OPCODE_CYCLES[0x12] = 8;
		OPCODE_CYCLES[0x13] = 19;
		OPCODE_CYCLES[0x14] = 13;
		OPCODE_CYCLES[0x16] = 44;
		OPCODE_CYCLES[0x1B] = 17;
		OPCODE_CYCLES[0x1C] = 11;
		OPCODE_CYCLES[0x1E] = 14;
		OPCODE_CYCLES[0x20] = 30;
		OPCODE_CYCLES[0x28] = 5;
		OPCODE_CYCLES[0x29] = 8;
		OPCODE_CYCLES[0x2A] = 17;
		OPCODE_CYCLES[0x2D] = 5;
		OPCODE_CYCLES[0x2E] = 6;
		OPCODE_CYCLES[0x30] = 23;
		OPCODE_CYCLES[0x3D] = 5;
		OPCODE_CYCLES[0x3E] = 5;
		OPCODE_CYCLES[0x3F] = 39;

		return OPCODE_CYCLES;
	}

	constexpr auto OPCODE_CYCLES = generate_opcode_cycles_table();

	void GTE::InterlockCommand() {
		auto current_time = m_sys_status->scheduler.GetTimestamp();
		if (current_time >= m_cmd_interlock) {
			return;
		}

		auto diff = (m_cmd_interlock - current_time);
		m_sys_status->sysbus->m_curr_cycles += diff;
	}

	u32 GTE::DivAndSaturate(u32 h, u32 sz) {
		constexpr auto generate_unr_table = [](){
			constexpr auto TABLE_SIZE = 257U;
			std::array<u32, TABLE_SIZE> unr_tab = {};

			for (auto idx = 0; idx < TABLE_SIZE; ++idx) {
				unr_tab[idx] = u32(std::max(
					0x00,
					(0x40000 / (idx + 0x100) + 1) / 2 - 0x101
				));
			}

			return unr_tab;
		};

		constexpr auto unr_table = generate_unr_table();

		if (h < (sz * 2)) {
			//Perform unsigned newton-raphson
			auto zero_count = std::countl_zero(u16(sz));
			auto numer      = h << zero_count;
			auto denom      = sz << zero_count;
			//Get reciprocal
			auto divisor    = unr_table[(denom - 0x7FC0) >> 7] + 0x101;
			//Two iterations
			denom = ((0x2000080 - (denom * divisor)) >> 8);
			denom = ((0x0000080 + (denom * divisor)) >> 8);
			return u32(std::min(0x1FFFFULL, ((u64(numer) * denom) + 0x8000) >> 16));
		}
		else {
			//Set flags, bit 17 and bit 31
			m_regs.flags.div_overflow = true;
			return 0x1FFFF;
		}
	}

	void GTE::Cmd(u32 cmd) {
		InterlockCommand();

		LOG_DEBUG("COP2", "[COP2] Command {:#010x}", cmd);

		//Reset all flags from prev command
		m_regs.flags.raw = 0;

		GTE_CmdEncoding encoding{};
		encoding.raw = cmd;

		switch (encoding.opcode())
		{
		case GTE_Opcode::RTPS:  RTPS(encoding, 0, true); break;
		case GTE_Opcode::NCLIP: NCLIP(encoding);       break;
		case GTE_Opcode::OP:    OP(encoding);          break;
		case GTE_Opcode::DPCS:  DPCS(encoding, false); break;
		case GTE_Opcode::INTPL: INTPL(encoding);       break;
		case GTE_Opcode::MVMVA: MVMVA(encoding);       break;
		case GTE_Opcode::NCDS:  NCDS(encoding, 0);     break;
		case GTE_Opcode::CDP:   CDP(encoding);         break;
		case GTE_Opcode::NCDT:  NCDT(encoding);        break;
		case GTE_Opcode::NCCS:  NCCS(encoding, 0);     break;
		case GTE_Opcode::CC:    CC(encoding);          break;
		case GTE_Opcode::NCS:   NCS(encoding, 0);      break;
		case GTE_Opcode::NCT:   NCT(encoding);         break;
		case GTE_Opcode::SQR:   SQR(encoding);         break;
		case GTE_Opcode::DPCL:  DPCL(encoding);        break;
		case GTE_Opcode::DPCT:  DPCT(encoding);        break;
		case GTE_Opcode::AVSZ3: AVSZ3(encoding);       break;
		case GTE_Opcode::AVSZ4: AVSZ4(encoding);       break;
		case GTE_Opcode::RTPT:  RTPT(encoding);        break;
		case GTE_Opcode::GPF:   GPF(encoding);         break;
		case GTE_Opcode::GPL:   GPL(encoding);         break;
		case GTE_Opcode::NCCT:  NCCT(encoding);        break;
		default:
			LOG_ERROR("COP2", "[COP2] N/A Opcode executed at {:#010x}",
				m_sys_status->cpu->GetPc());
			break;
		}

		m_cmd_interlock = m_sys_status->scheduler.GetTimestamp() + 
			OPCODE_CYCLES[u8(encoding.opcode())];
	}

	void GTE::MoveScreenFifo() {
		m_regs.sxy0 = m_regs.sxy1;
		m_regs.sxy1 = m_regs.sxy2;
	}

	void GTE::PushScreenFifo(GTE_Vec2<i16> coord) {
		MoveScreenFifo();
		m_regs.sxy2 = coord;
	}

	void GTE::MoveZFifo() {
		m_regs.sz0 = m_regs.sz1;
		m_regs.sz1 = m_regs.sz2;
		m_regs.sz2 = m_regs.sz3;
	}

	void GTE::PushZFifo(u16 coord) {
		MoveZFifo();
		m_regs.sz3.vec[0] = coord;
	}

	void GTE::MoveColorFifo() {
		m_regs.rgb0 = m_regs.rgb1;
		m_regs.rgb1 = m_regs.rgb2;
	}

	void GTE::PushColorFifo(GTE_Vec3<u32> color) {
		MoveColorFifo();
		
		auto SaturateColor = [this](u8 color_id, int32_t value) {
			if (value > 0xFF) {
				m_regs.flags.raw |= 1 << (21 - color_id);
				return 0xFF;
			}

			if (value < 0) {
				m_regs.flags.raw |= 1 << (21 - color_id);
				return 0;
			}

			return value;
		};

		GTE_Vec4<u8> fifo_entry{};
		fifo_entry.vec[0] = u8(SaturateColor(0, color.vec[0]));
		fifo_entry.vec[1] = u8(SaturateColor(1, color.vec[1]));
		fifo_entry.vec[2] = u8(SaturateColor(2, color.vec[2]));
		fifo_entry.vec[3] = m_regs.color_value.vec[3];

		m_regs.rgb2 = fifo_entry;
	}

	void GTE::PushColorFromMac() {
		GTE_Vec3<u32> color{};
		color.vec[0] = m_regs.mac1_3.vec[0] >> 4;
		color.vec[1] = m_regs.mac1_3.vec[1] >> 4;
		color.vec[2] = m_regs.mac1_3.vec[2] >> 4;
		PushColorFifo(color);
	}
}