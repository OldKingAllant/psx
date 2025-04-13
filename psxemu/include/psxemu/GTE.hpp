#pragma once

#include <common/Defs.hpp>
#include <common/Macros.hpp>

#include <numeric>

#include "GTEStructs.hpp"

namespace psx {
	struct system_status;
}

namespace psx::cpu {
	union GTE_Regs {
#pragma pack(push, 1)
		struct {
			GTE_Vec3<i16>   v0;
			u16 _unused0;
			GTE_Vec3<i16>   v1;
			u16 _unused1;
			GTE_Vec3<i16>   v2;
			u16 _unused2;
			GTE_Vec4<u8>    color_value;
			GTE_Vec2<u16>   avg_z;
			GTE_Vec2<i16>   ir0;
			GTE_Vec2<i16>   ir1;
			GTE_Vec2<i16>   ir2;
			GTE_Vec2<i16>   ir3;
			GTE_Vec2<i16>   sxy0;
			GTE_Vec2<i16>   sxy1;
			GTE_Vec2<i16>   sxy2;
			u32 _unused3;
			GTE_Vec2<u16>   sz0;
			GTE_Vec2<u16>   sz1;
			GTE_Vec2<u16>   sz2;
			GTE_Vec2<u16>   sz3;

			GTE_Vec4<u8>    rgb0;
			GTE_Vec4<u8>    rgb1;
			GTE_Vec4<u8>    rgb2;

			GTE_Vec4<u8>    res1;

			i32 mac0;
			GTE_Vec3<i32> mac1_3;
			GTE_Vec2<u32> irgb;
			GTE_Vec2<i32> lzc;

			/////////////////////////////

			GTE_Matrix      rotation_matrix;
			GTE_Vec3<i32>   translation_vec;
			GTE_Matrix      light_matrix;
			GTE_Vec3<i32>   bg_color_vec;
			GTE_Matrix      light_color_mat;
			GTE_Vec3<i32>   far_color_vec;
			GTE_Vec2<i32>   screen_offset;
			GTE_Vec2<u16>   proj_plane_dist;
			i16 dqa;
			u16 _unused;
			i32 dqb;
			GTE_Vec2<i16>  avg_z_scale_0;
			GTE_Vec2<i16>  avg_z_scale_1;
			GTE_Flags      flags;
		};
#pragma pack(pop)

		u32 array[64];

		static constexpr u8 DATA_REGS_BASE    = 0;
		static constexpr u8 CONTROL_REGS_BASE = 32;
	};

	class GTE {
	public :
		GTE(system_status* sys_status);

		u32 ReadData(u8 reg);
		u32 ReadControl(u8 reg);

		void WriteData(u8 reg, u32 val);
		void WriteControl(u8 reg, u32 val);

		void Cmd(u32 cmd);

		void MoveScreenFifo();
		void PushScreenFifo(GTE_Vec2<i16> coord);

		void MoveZFifo();
		void PushZFifo(u16 coord);

		void MoveColorFifo();
		void PushColorFifo(GTE_Vec3<u32> color);
		void PushColorFromMac();

	private :
		void RTPS (GTE_CmdEncoding, u8 vec_number, bool set_mac0);
		void NCLIP(GTE_CmdEncoding);
		void OP   (GTE_CmdEncoding);
		void DPCS (GTE_CmdEncoding, bool use_fifo);
		void INTPL(GTE_CmdEncoding);
		void MVMVA(GTE_CmdEncoding);
		void NCDS (GTE_CmdEncoding, u8 vec_number);
		void CDP  (GTE_CmdEncoding);
		void NCDT (GTE_CmdEncoding);
		void NCCS (GTE_CmdEncoding, u8 vec_number);
		void CC   (GTE_CmdEncoding);
		void NCS  (GTE_CmdEncoding, u8 vec_number);
		void NCT  (GTE_CmdEncoding);
		void SQR  (GTE_CmdEncoding);
		void DPCL (GTE_CmdEncoding);
		void DPCT (GTE_CmdEncoding);
		void AVSZ3(GTE_CmdEncoding);
		void AVSZ4(GTE_CmdEncoding);
		void RTPT (GTE_CmdEncoding);
		void GPF  (GTE_CmdEncoding);
		void GPL  (GTE_CmdEncoding);
		void NCCT (GTE_CmdEncoding);

		void InterlockCommand();

		u32 DivAndSaturate(u32 h, u32 sz);

		template <u8 MacNum>
		i64 SetMac(i64 value) {
			static_assert(0 < MacNum && MacNum < 4);
			constexpr auto MAX_VALUE =  (1LL << 43ULL);
			constexpr auto MIN_VALUE = -(1LL << 43ULL);
			if (value >= MAX_VALUE) {
				m_regs.flags.raw |= (1 << (31 - MacNum));
			}
			else if(value < MIN_VALUE) {
				m_regs.flags.raw |= (1 << (28 - MacNum));
			}
			m_regs.mac1_3.vec[MacNum - 1] = i32(value);
			return value;
		}

		template <u8 MacNum>
		i64 CheckMacOverflow(i64 value) {
			static_assert(0 < MacNum && MacNum < 4);
			constexpr auto MAX_VALUE = (1LL << 43ULL);
			constexpr auto MIN_VALUE = -(1LL << 43ULL);
			if (value >= MAX_VALUE) {
				m_regs.flags.raw |= (1 << (31 - MacNum));
			}
			else if (value < MIN_VALUE) {
				m_regs.flags.raw |= (1 << (28 - MacNum));
			}
			return sign_extend<i64, 43>(value);
		}

		template <u8 IrNum> 
		i16 SaturateIR(i32 value, bool lm) {
			static_assert(0 < IrNum && IrNum < 4);

			auto final_value = i16(value);

			auto clamp = [](auto value, auto min, auto max) {
				if (value < min)
					return min;
				if (value > max)
					return max;
				return value;
			};

			if ((value < 0 || value > 0x7FFF) && lm) {
				m_regs.flags.raw |= (1 << (25 - IrNum));
				final_value = i16(clamp(value, 0, 0x7FFF));
			}
			else if((value < -0x8000 || value > 0x7FFF) && !lm) {
				m_regs.flags.raw |= (1 << (25 - IrNum));
				final_value = i16(clamp(value, -0x8000, 0x7FFF));
			}

			RGB_555 color{}; 

			if constexpr (IrNum == 1) {
				m_regs.ir1.vec[0] = final_value;
			}
			else if constexpr (IrNum == 2) {
				m_regs.ir2.vec[0] = final_value;
			}
			else if constexpr (IrNum == 3) {
				m_regs.ir3.vec[0] = final_value;
			}

			color.set_red(  clamp(m_regs.ir1.vec[0] / 0x80, 0x0, 0x1F));
			color.set_green(clamp(m_regs.ir2.vec[0] / 0x80, 0x0, 0x1F));
			color.set_blue( clamp(m_regs.ir3.vec[0] / 0x80, 0x0, 0x1F));

			m_regs.irgb.vec[0] = u16(color);
			m_regs.irgb.vec[1] = u16(color);

			return final_value;
		}

		u16 SaturateSZ3(i32 value) {
			auto clamp = [](auto value, auto min, auto max) {
				if (value < min)
					return min;
				if (value > max)
					return max;
				return value;
			};

			if (value < 0 || value > 0xFFFF) {
				m_regs.flags.otz_saturated = true;
				return u16(clamp(value, 0, 0xFFFF));
			}
			return u16(value);
		}

		i64 SetMac0(i64 value) {
			constexpr auto MAX_VALUE =  (1LL << 31ULL);
			constexpr auto MIN_VALUE = -(1LL << 31ULL);
			if (value >= MAX_VALUE) {
				m_regs.flags.mac0_large_pos = true;
			}
			else if(value < MIN_VALUE) {
				m_regs.flags.mac0_large_neg = true;
			}
			m_regs.mac0 = i32(value);
			return value;
		}

		i16 SaturateSX2(i32 value) {
			auto clamp = [](auto value, auto min, auto max) {
				if (value < min)
					return min;
				if (value > max)
					return max;
				return value;
			};

			if (value < -1024 || value > 0x3FF) {
				m_regs.flags.sx2_saturated = true;
				return i16(clamp(value, -1024, 0x3FF));
			}

			return i16(value);
		}

		i16 SaturateSY2(i32 value) {
			auto clamp = [](auto value, auto min, auto max) {
				if (value < min)
					return min;
				if (value > max)
					return max;
				return value;
			};

			if (value < -1024 || value > 0x3FF) {
				m_regs.flags.sy2_saturated = true;
				return i16(clamp(value, -1024, 0x3FF));
			}

			return i16(value);
		}

		i16 SaturateIR0(i64 value) {
			auto clamp = [](auto value, auto min, auto max) {
				if (value < min)
					return min;
				if (value > max)
					return max;
				return value;
			};

			auto final_value = i16(value);
			if (value < 0 || value > 0x1000) {
				m_regs.flags.ir0_saturated = true;
				final_value = i16(clamp(value, 0LL, 0x1000LL));
			}

			m_regs.ir0.vec[0] = final_value;
			return final_value;
		}

		void SaturateOTZ(i64 value) {
			auto clamp = [](auto value, auto min, auto max) {
				if (value < min)
					return min;
				if (value > max)
					return max;
				return value;
			};

			auto final_value = u16(value);
			if (value < 0 || value > 0xFFFF) {
				m_regs.flags.otz_saturated = true;
				final_value = u16(clamp(value, 0LL, 0xFFFFLL));
			}

			m_regs.avg_z.vec[0] = final_value;
		}

		void VectorMul(GTE_Vec3<i16>, GTE_Vec3<i16>, GTE_Vec3<i16>, bool lm, u32 sf);
		void VectorMatMul(GTE_Matrix, GTE_Vec3<i16>, GTE_Vec3<i32>, bool lm, u32 sf);
		void Interpolate(i64 mac1, i64 mac2, i64 mac3, bool lm, u32 sf);

		void UpdateIRGB();

		GTE_Vec3<i16> IRToVec() const {
			GTE_Vec3<i16> vec{};
			vec.vec[0] = m_regs.ir1.vec[0];
			vec.vec[1] = m_regs.ir2.vec[0];
			vec.vec[2] = m_regs.ir3.vec[0];
			return vec;
		}

		GTE_Vec3<i16> IR0ToVec() const {
			GTE_Vec3<i16> vec{};
			vec.vec[0] = m_regs.ir0.vec[0];
			vec.vec[1] = m_regs.ir0.vec[0];
			vec.vec[2] = m_regs.ir0.vec[0];
			return vec;
		}

	private :
		GTE_Regs       m_regs;
		system_status* m_sys_status;
		u64            m_cmd_interlock;
	};
}