#pragma once

#include <common/Defs.hpp>
#include <common/Macros.hpp>

namespace psx::cpu {
	struct GTE_Fixed16 {
		u16 raw;

		FORCE_INLINE constexpr u8 get_sign() const {
			return u8((raw >> 15) & 1);
		}

		FORCE_INLINE constexpr u8 get_int() const {
			return u8((raw >> 12) & 7);
		}

		FORCE_INLINE constexpr u16 get_fract() const {
			return u16(raw & 0xFFF);
		}

		FORCE_INLINE constexpr void set_sign(u8 sign) {
			raw &= ~(1 << 15);
			raw |= u16(sign) << 15;
		}

		FORCE_INLINE constexpr void set_int(u8 int_part) {
			raw &= ~(7 << 12);
			raw |= u16(int_part) << 12;
		}

		FORCE_INLINE constexpr void set_fract(u16 fract) {
			raw &= ~0xFFF;
			raw |= (fract & 0xFFF);
		}

		constexpr explicit operator u16() const {
			return raw;
		}

		constexpr explicit operator u32() const {
			return u32(sign_extend<i32, 15>(raw));
		}
	};

	struct GTE_Matrix {
		i16 mat[3][3];
		u16 _unused;

		FORCE_INLINE constexpr u32 read_last() const {
			return u32(mat[2][2]);
		}
	};

	template <typename Ty>
	struct GTE_Vec3 {
		Ty vec[3];
	};

	template <typename Ty>
	struct GTE_Vec2 {
		Ty vec[2];
	};

	template <typename Ty>
	struct GTE_Vec4 {
		Ty vec[4];
	};

	struct RGB_555 {
	private :
		u8 r, g, b;

	public:
		RGB_555()               = default;
		RGB_555(RGB_555&&)      = default;
		RGB_555(RGB_555 const&) = default;

		RGB_555(auto value) :
			RGB_555() {
			*this = value;
		}

		FORCE_INLINE constexpr void set_red(auto value) {
			r = u8(value);
		}

		FORCE_INLINE constexpr void set_green(auto value) {
			g = u8(value);
		}

		FORCE_INLINE constexpr void set_blue(auto value) {
			b = u8(value);
		}

		FORCE_INLINE constexpr auto get_red() const -> decltype(r) {
			return r;
		}

		FORCE_INLINE constexpr auto get_green() const -> decltype(r) {
			return g;
		}

		FORCE_INLINE constexpr auto get_blue() const -> decltype(r) {
			return b;
		}

		FORCE_INLINE constexpr explicit operator u16() const {
			return
				u16(r << 00) |
				u16(g << 05) |
				u16(b << 10);
		}

		FORCE_INLINE RGB_555& operator=(auto value) {
			r = (value >> 00) & 0x1F;
			g = (value >> 05) & 0x1F;
			b = (value >> 10) & 0x1F;
			return *this;
		}
	};

	enum class GTE_Opcode : u8 {
		RTPS  = 0x01,
		NCLIP = 0x06,
		OP	  = 0x0C,
		DPCS  = 0x10,
		INTPL = 0x11,
		MVMVA = 0x12,
		NCDS  = 0x13,
		CDP   = 0x14,
		NCDT  = 0x16,
		NCCS  = 0x1B,
		CC    = 0x1C,
		NCS   = 0x1E,
		NCT   = 0x20,
		SQR   = 0x28,
		DPCL  = 0x29,
		DPCT  = 0x2A,
		AVSZ3 = 0x2D,
		AVSZ4 = 0x2E,
		RTPT  = 0x30,
		GPF   = 0x3D,
		GPL   = 0x3E,
		NCCT  = 0x3F
	};

	enum class GTE_IR_Saturation : u8 {
		NEGATIVE,
		POSITIVE
	};

	enum class GTE_MVMVA_Translation : u8 {
		TR,
		BK,
		FC,
		NONE
	};

	enum class GTE_MVMVA_MulVec : u8 {
		V0,
		V1,
		V2,
		IR
	};

	enum class GTE_MVMVA_MulMat : u8 {
		ROT,
		LIT,
		COL,
		INV
	};

	enum class GTE_IR_Shift : u8 {
		NO_FRACTION,
		FRACT_12BIT
	};

	struct GTE_CmdEncoding {
		u32 raw;

		constexpr auto opcode() const -> GTE_Opcode {
			return GTE_Opcode(raw & 0x3F);
		}

		constexpr auto lm() const -> GTE_IR_Saturation {
			return GTE_IR_Saturation((raw >> 10) & 1);
		}

		constexpr auto translation() const -> GTE_MVMVA_Translation {
			return GTE_MVMVA_Translation((raw >> 13) & 0x3);
		}

		constexpr auto vec() const -> GTE_MVMVA_MulVec {
			return GTE_MVMVA_MulVec((raw >> 15) & 0x3);
		}

		constexpr auto mat() const -> GTE_MVMVA_MulMat {
			return GTE_MVMVA_MulMat((raw >> 17) & 0x3);
		}

		constexpr auto sf() const -> GTE_IR_Shift {
			return GTE_IR_Shift((raw >> 19) & 1);
		}

		constexpr auto fake() const -> u8 {
			return u8((raw >> 20) & 0x1F);
		}
	};

	union GTE_Flags {
#pragma pack(push, 1)
		struct {
			u8 : 8;
			u8 : 4;
			bool ir0_saturated    : 1;
			bool sy2_saturated    : 1;
			bool sx2_saturated    : 1;
			bool mac0_large_neg   : 1;
			bool mac0_large_pos   : 1;
			bool div_overflow     : 1;
			bool otz_saturated    : 1;
			bool fifo_b_saturated : 1;
			bool fifo_g_saturated : 1;
			bool fifo_r_saturated : 1;
			bool ir3_saturated	  : 1;
			bool ir2_saturated    : 1;
			bool ir1_saturated    : 1;
			bool mac3_negative    : 1;
			bool mac2_negative    : 1;
			bool mac1_negative    : 1;
			bool mac3_positive    : 1;
			bool mac2_positive    : 1;
			bool mac1_positive    : 1;
		};
#pragma pack(pop)

		u32 raw;
	};
}