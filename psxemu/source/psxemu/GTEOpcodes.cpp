#include <psxemu/include/psxemu/GTE.hpp>
#include <psxemu/include/psxemu/SystemStatus.hpp>

#include <psxemu/include/psxemu/SystemBus.hpp>

#include <common/Errors.hpp>

namespace psx::cpu {

	void GTE::RTPS(GTE_CmdEncoding cmd, u8 vec_number, bool set_mac0) {
		auto sf = u32(cmd.sf());
		auto lm = bool(cmd.lm());

		auto tr = m_regs.translation_vec;
		auto rt = m_regs.rotation_matrix;

		GTE_Vec3<i16> v0{};

		switch (vec_number)
		{
		case 0:
			v0 = m_regs.v0;
			break;
		case 1:
			v0 = m_regs.v1;
			break;
		case 2:
			v0 = m_regs.v2;
			break;
		default:
			error::Unreachable();
			break;
		}

		auto temp1 = CheckMacOverflow<1>(CheckMacOverflow<1>(CheckMacOverflow<1>(i64(tr.vec[0]) * 0x1000 + i64(rt.mat[0][0]) * v0.vec[0]) + i64(rt.mat[0][1]) * v0.vec[1]) + i64(rt.mat[0][2]) * v0.vec[2]);
		auto ir1 = SaturateIR<1>(i32(SetMac<1>(temp1 >> (sf * 12))), lm);
		auto temp2 = CheckMacOverflow<2>(CheckMacOverflow<2>(CheckMacOverflow<2>(i64(tr.vec[1]) * 0x1000 + i64(rt.mat[1][0]) * v0.vec[0]) + i64(rt.mat[1][1]) * v0.vec[1]) + i64(rt.mat[1][2]) * v0.vec[2]);
		auto ir2 = SaturateIR<2>(i32(SetMac<2>(temp2 >> (sf * 12))), lm);
		auto temp3 = CheckMacOverflow<3>(CheckMacOverflow<3>(CheckMacOverflow<3>(i64(tr.vec[2]) * 0x1000 + i64(rt.mat[2][0]) * v0.vec[0]) + i64(rt.mat[2][1]) * v0.vec[1]) + i64(rt.mat[2][2]) * v0.vec[2]);
		
		auto mac3 = SetMac<3>(temp3 >> (sf * 12));
		auto mac3_fract = temp3 >> 12;

		if (mac3_fract < -0x8000 || mac3_fract > 0x7FFF) {
			m_regs.flags.ir3_saturated = true;
		}

		auto clamp = [](auto value, auto min, auto max) {
			if (value < min)
				return min;
			if (value > max)
				return max;
			return value;
		};

		m_regs.ir3.vec[0] = i16(clamp(m_regs.mac1_3.vec[2], lm ? 0 : -0x8000, 0x7FFF));
		//auto ir3 = SaturateIR<3>(SetMac<3>(temp3), false);

		auto sz3 = SaturateSZ3(i32(temp3 >> 12));
		auto h = m_regs.proj_plane_dist.vec[0];

		auto ofx = m_regs.screen_offset.vec[0];
		auto ofy = m_regs.screen_offset.vec[1];

		auto dqa = m_regs.dqa;
		auto dqb = m_regs.dqb;

		auto div_result = i64(DivAndSaturate(h, sz3));

		auto mac0 = int64_t(0);

		mac0 = SetMac0(div_result * ir1 + ofx);
		auto sx2 = SaturateSX2(i32(mac0 >> 16));
		mac0 = SetMac0(div_result * ir2 + ofy);
		auto sy2 = SaturateSY2(i32(mac0 >> 16));

		if (set_mac0) {
			mac0 = SetMac0(div_result * dqa + dqb);
			SaturateIR0(mac0 >> 12);
		}

		GTE_Vec2<i16> screen_xy{};
		screen_xy.vec[0] = sx2; screen_xy.vec[1] = sy2;
		PushScreenFifo(screen_xy);
		PushZFifo(sz3);

		UpdateIRGB();
	}

	void GTE::NCLIP(GTE_CmdEncoding) {
		auto temp =
			int64_t(m_regs.sxy0.vec[0]) * m_regs.sxy1.vec[1] +
			int64_t(m_regs.sxy1.vec[0]) * m_regs.sxy2.vec[1] +
			int64_t(m_regs.sxy2.vec[0]) * m_regs.sxy0.vec[1] -
			int64_t(m_regs.sxy0.vec[0]) * m_regs.sxy2.vec[1] -
			int64_t(m_regs.sxy1.vec[0]) * m_regs.sxy0.vec[1] -
			int64_t(m_regs.sxy2.vec[0]) * m_regs.sxy1.vec[1];
		SetMac0(temp);
	}

	void GTE::OP(GTE_CmdEncoding cmd) {
		auto d1 = m_regs.rotation_matrix.mat[0][0];
		auto d2 = m_regs.rotation_matrix.mat[1][1];
		auto d3 = m_regs.rotation_matrix.mat[2][2];

		auto ir1 = m_regs.ir1.vec[0];
		auto ir2 = m_regs.ir2.vec[0];
		auto ir3 = m_regs.ir3.vec[0];

		auto lm = bool(cmd.lm());
		auto sf = u32(cmd.sf());

		auto temp1 = CheckMacOverflow<1>(i64(ir3) * d2 - i64(ir2) * d3);
		auto temp2 = CheckMacOverflow<2>(i64(ir1) * d3 - i64(ir3) * d1);
		auto temp3 = CheckMacOverflow<3>(i64(ir2) * d1 - i64(ir1) * d2);

		SaturateIR<1>(i32(SetMac<1>(temp1 >> (sf * 12))), lm);
		SaturateIR<2>(i32(SetMac<2>(temp2 >> (sf * 12))), lm);
		SaturateIR<3>(i32(SetMac<3>(temp3 >> (sf * 12))), lm);

		UpdateIRGB();
	}
	
	void GTE::DPCS(GTE_CmdEncoding cmd, bool use_fifo) {
		auto lm = bool(cmd.lm());
		auto sf = u32(cmd.sf());

		GTE_Vec4<u8> input_color{};

		if (use_fifo) {
			input_color = m_regs.rgb0;
		}
		else {
			input_color = m_regs.color_value;
		}
		
		auto r = i16(input_color.vec[0]) << 4;
		auto g = i16(input_color.vec[1]) << 4;
		auto b = i16(input_color.vec[2]) << 4;
		
		auto fc = m_regs.far_color_vec;

		auto temp1 = CheckMacOverflow<1>((i64(fc.vec[0]) << 12) - (i64(r) << 12));
		auto temp2 = CheckMacOverflow<2>((i64(fc.vec[1]) << 12) - (i64(g) << 12));
		auto temp3 = CheckMacOverflow<3>((i64(fc.vec[2]) << 12) - (i64(b) << 12));
		SaturateIR<1>(i32(SetMac<1>(temp1 >> (12 * sf))), false);
		SaturateIR<2>(i32(SetMac<2>(temp2 >> (12 * sf))), false);
		SaturateIR<3>(i32(SetMac<3>(temp3 >> (12 * sf))), false);

		GTE_Vec3<i16> color_vec{};
		color_vec.vec[0] = r;
		color_vec.vec[1] = g;
		color_vec.vec[2] = b;

		VectorMul(IR0ToVec(), IRToVec(), color_vec, lm, sf);

		PushColorFromMac();
		UpdateIRGB();
	}

	void GTE::INTPL(GTE_CmdEncoding cmd) {
		auto lm = bool(cmd.lm());
		auto sf = u32(cmd.sf());

		auto r = i16(m_regs.ir1.vec[0]);
		auto g = i16(m_regs.ir2.vec[0]);
		auto b = i16(m_regs.ir3.vec[0]);

		auto fc = m_regs.far_color_vec;

		auto temp1 = CheckMacOverflow<1>((i64(fc.vec[0]) << 12) - (i64(r) << 12));
		auto temp2 = CheckMacOverflow<2>((i64(fc.vec[1]) << 12) - (i64(g) << 12));
		auto temp3 = CheckMacOverflow<3>((i64(fc.vec[2]) << 12) - (i64(b) << 12));
		SaturateIR<1>(i32(SetMac<1>(temp1 >> (12 * sf))), false);
		SaturateIR<2>(i32(SetMac<2>(temp2 >> (12 * sf))), false);
		SaturateIR<3>(i32(SetMac<3>(temp3 >> (12 * sf))), false);

		GTE_Vec3<i16> color_vec{};
		color_vec.vec[0] = r;
		color_vec.vec[1] = g;
		color_vec.vec[2] = b;

		VectorMul(IR0ToVec(), IRToVec(), color_vec, lm, sf);

		PushColorFromMac();
		UpdateIRGB();
	}

	void GTE::MVMVA(GTE_CmdEncoding cmd) {
		auto sf = u32(cmd.sf());
		auto lm = bool(cmd.lm());

		auto mx_select = cmd.mat();
		auto vx_select = cmd.vec();
		auto tx_select = cmd.translation();

		GTE_Matrix    mx{};
		GTE_Vec3<i16> vx{};
		GTE_Vec3<i32> tx{};

		switch (mx_select)
		{
		case GTE_MVMVA_MulMat::ROT:
			mx = m_regs.rotation_matrix;
			break;
		case GTE_MVMVA_MulMat::LIT:
			mx = m_regs.light_matrix;
			break;
		case GTE_MVMVA_MulMat::COL:
			mx = m_regs.light_color_mat;
			break;
		case GTE_MVMVA_MulMat::INV:
		{
			auto r = i16(m_regs.color_value.vec[0]) << 4;

			mx.mat[0][0] = -r;
			mx.mat[0][1] =  r;
			mx.mat[0][2] = m_regs.ir0.vec[0];

			mx.mat[1][0] = m_regs.rotation_matrix.mat[0][2];
			mx.mat[1][1] = m_regs.rotation_matrix.mat[0][2];
			mx.mat[1][2] = m_regs.rotation_matrix.mat[0][2];

			mx.mat[2][0] = m_regs.rotation_matrix.mat[1][1];
			mx.mat[2][1] = m_regs.rotation_matrix.mat[1][1];
			mx.mat[2][2] = m_regs.rotation_matrix.mat[1][1];
		}
			break;
		default:
			error::Unreachable();
			break;
		}

		switch (vx_select)
		{
		case GTE_MVMVA_MulVec::V0:
			vx = m_regs.v0;
			break;
		case GTE_MVMVA_MulVec::V1:
			vx = m_regs.v1;
			break;
		case GTE_MVMVA_MulVec::V2:
			vx = m_regs.v2;
			break;
		case GTE_MVMVA_MulVec::IR:
			vx.vec[0] = m_regs.ir1.vec[0];
			vx.vec[1] = m_regs.ir2.vec[0];
			vx.vec[2] = m_regs.ir3.vec[0];
			break;
		default:
			error::Unreachable();
			break;
		}

		switch (tx_select)
		{
		case GTE_MVMVA_Translation::TR:
			tx = m_regs.translation_vec;
			break;
		case GTE_MVMVA_Translation::BK:
			tx = m_regs.bg_color_vec;
			break;
		case GTE_MVMVA_Translation::FC:
			tx = m_regs.far_color_vec;
			break;
		case GTE_MVMVA_Translation::NONE:
			break;
		default:
			error::Unreachable();
			break;
		}


		if (tx_select == GTE_MVMVA_Translation::FC) {
			SaturateIR<1>(i32(CheckMacOverflow<1>(i64(tx.vec[0]) * 0x1000 + i64(mx.mat[0][0]) * vx.vec[0]) >> (12 * sf)), false);
			SaturateIR<2>(i32(CheckMacOverflow<2>(i64(tx.vec[1]) * 0x1000 + i64(mx.mat[1][0]) * vx.vec[0]) >> (12 * sf)), false);
			SaturateIR<3>(i32(CheckMacOverflow<3>(i64(tx.vec[2]) * 0x1000 + i64(mx.mat[2][0]) * vx.vec[0]) >> (12 * sf)), false);

			auto temp1 = CheckMacOverflow<1>(CheckMacOverflow<1>(i64(mx.mat[0][1]) * vx.vec[1]) + i64(mx.mat[0][2]) * vx.vec[2]);
			auto temp2 = CheckMacOverflow<2>(CheckMacOverflow<2>(i64(mx.mat[1][1]) * vx.vec[1]) + i64(mx.mat[1][2]) * vx.vec[2]);
			auto temp3 = CheckMacOverflow<3>(CheckMacOverflow<3>(i64(mx.mat[2][1]) * vx.vec[1]) + i64(mx.mat[2][2]) * vx.vec[2]);

			SaturateIR<1>(i32(SetMac<1>(temp1 >> (12 * sf))), lm);
			SaturateIR<2>(i32(SetMac<2>(temp2 >> (12 * sf))), lm);
			SaturateIR<3>(i32(SetMac<3>(temp3 >> (12 * sf))), lm);
		}
		else {
			VectorMatMul(mx, vx, tx, lm, sf);
		}

		UpdateIRGB();
	}

	void GTE::NCDS(GTE_CmdEncoding cmd, u8 vec_number) {
		auto sf = u32(cmd.sf());
		auto lm = bool(cmd.lm());

		auto llm = m_regs.light_matrix;
		auto bk = m_regs.bg_color_vec;
		auto lcm = m_regs.light_color_mat;

		GTE_Vec3<i16> vx{};

		switch (vec_number)
		{
		case 0:
			vx = m_regs.v0;
			break;
		case 1:
			vx = m_regs.v1;
			break;
		case 2:
			vx = m_regs.v2;
			break;
		default:
			error::Unreachable();
			break;
		}

		VectorMatMul(llm, vx, {}, lm, sf);
		VectorMatMul(lcm, IRToVec(), bk, lm, sf);

		auto r = i64(m_regs.color_value.vec[0]);
		auto g = i64(m_regs.color_value.vec[1]);
		auto b = i64(m_regs.color_value.vec[2]);

		auto fc = m_regs.far_color_vec;

		auto prev_mac1 = SetMac<1>(CheckMacOverflow<1>((r * m_regs.ir1.vec[0]) << 4));
		auto prev_mac2 = SetMac<2>(CheckMacOverflow<2>((g * m_regs.ir2.vec[0]) << 4));
		auto prev_mac3 = SetMac<3>(CheckMacOverflow<3>((b * m_regs.ir3.vec[0]) << 4));

		SaturateIR<1>(i32(SetMac<1>(CheckMacOverflow<1>((i64(fc.vec[0]) << 12) - prev_mac1) >> (12 * sf))), false);
		SaturateIR<2>(i32(SetMac<2>(CheckMacOverflow<2>((i64(fc.vec[1]) << 12) - prev_mac2) >> (12 * sf))), false);
		SaturateIR<3>(i32(SetMac<3>(CheckMacOverflow<3>((i64(fc.vec[2]) << 12) - prev_mac3) >> (12 * sf))), false);

		SaturateIR<1>(i32(SetMac<1>(CheckMacOverflow<1>(prev_mac1 + i64(m_regs.ir1.vec[0]) * m_regs.ir0.vec[0]) >> (12 * sf))), lm);
		SaturateIR<2>(i32(SetMac<2>(CheckMacOverflow<2>(prev_mac2 + i64(m_regs.ir2.vec[0]) * m_regs.ir0.vec[0]) >> (12 * sf))), lm);
		SaturateIR<3>(i32(SetMac<3>(CheckMacOverflow<3>(prev_mac3 + i64(m_regs.ir3.vec[0]) * m_regs.ir0.vec[0]) >> (12 * sf))), lm);

		PushColorFromMac();
		UpdateIRGB();
	}

	void GTE::CDP(GTE_CmdEncoding cmd) {
		auto sf = u32(cmd.sf());
		auto lm = bool(cmd.lm());

		auto bk = m_regs.bg_color_vec;
		auto lcm = m_regs.light_color_mat;

		VectorMatMul(lcm, IRToVec(), bk, lm, sf);

		auto r = i64(m_regs.color_value.vec[0]);
		auto g = i64(m_regs.color_value.vec[1]);
		auto b = i64(m_regs.color_value.vec[2]);

		auto fc = m_regs.far_color_vec;

		auto prev_mac1 = SetMac<1>(CheckMacOverflow<1>((r * m_regs.ir1.vec[0]) << 4));
		auto prev_mac2 = SetMac<2>(CheckMacOverflow<2>((g * m_regs.ir2.vec[0]) << 4));
		auto prev_mac3 = SetMac<3>(CheckMacOverflow<3>((b * m_regs.ir3.vec[0]) << 4));

		SaturateIR<1>(i32(SetMac<1>(CheckMacOverflow<1>((i64(fc.vec[0]) << 12) - prev_mac1) >> (12 * sf))), false);
		SaturateIR<2>(i32(SetMac<2>(CheckMacOverflow<2>((i64(fc.vec[1]) << 12) - prev_mac2) >> (12 * sf))), false);
		SaturateIR<3>(i32(SetMac<3>(CheckMacOverflow<3>((i64(fc.vec[2]) << 12) - prev_mac3) >> (12 * sf))), false);

		SaturateIR<1>(i32(SetMac<1>(CheckMacOverflow<1>(prev_mac1 + i64(m_regs.ir1.vec[0]) * m_regs.ir0.vec[0]) >> (12 * sf))), lm);
		SaturateIR<2>(i32(SetMac<2>(CheckMacOverflow<2>(prev_mac2 + i64(m_regs.ir2.vec[0]) * m_regs.ir0.vec[0]) >> (12 * sf))), lm);
		SaturateIR<3>(i32(SetMac<3>(CheckMacOverflow<3>(prev_mac3 + i64(m_regs.ir3.vec[0]) * m_regs.ir0.vec[0]) >> (12 * sf))), lm);

		PushColorFromMac();
		UpdateIRGB();
	}

	void GTE::NCDT(GTE_CmdEncoding cmd) {
		NCDS(cmd, 0);
		NCDS(cmd, 1);
		NCDS(cmd, 2);
	}

	void GTE::NCCS(GTE_CmdEncoding cmd, u8 vec_number) {
		auto sf = u32(cmd.sf());
		auto lm = bool(cmd.lm());

		auto llm = m_regs.light_matrix;
		auto bk = m_regs.bg_color_vec;
		auto lcm = m_regs.light_color_mat;

		GTE_Vec3<i16> vx{};

		switch (vec_number)
		{
		case 0:
			vx = m_regs.v0;
			break;
		case 1:
			vx = m_regs.v1;
			break;
		case 2:
			vx = m_regs.v2;
			break;
		default:
			error::Unreachable();
			break;
		}

		VectorMatMul(llm, vx, {}, lm, sf);
		VectorMatMul(lcm, IRToVec(), bk, lm, sf);

		auto r = i64(m_regs.color_value.vec[0]);
		auto g = i64(m_regs.color_value.vec[1]);
		auto b = i64(m_regs.color_value.vec[2]);

		auto fc = m_regs.far_color_vec;

		auto prev_mac1 = SetMac<1>(CheckMacOverflow<1>((r * m_regs.ir1.vec[0]) << 4));
		auto prev_mac2 = SetMac<2>(CheckMacOverflow<2>((g * m_regs.ir2.vec[0]) << 4));
		auto prev_mac3 = SetMac<3>(CheckMacOverflow<3>((b * m_regs.ir3.vec[0]) << 4));

		SaturateIR<1>(i32(SetMac<1>(prev_mac1 >> (12 * sf))), lm);
		SaturateIR<2>(i32(SetMac<2>(prev_mac2 >> (12 * sf))), lm);
		SaturateIR<3>(i32(SetMac<3>(prev_mac3 >> (12 * sf))), lm);

		PushColorFromMac();
		UpdateIRGB();
	}

	void GTE::CC(GTE_CmdEncoding cmd) {
		auto sf = u32(cmd.sf());
		auto lm = bool(cmd.lm()); //lm is as if it was always 1

		auto bk = m_regs.bg_color_vec;
		auto lcm = m_regs.light_color_mat;

		VectorMatMul(lcm, IRToVec(), bk, lm, sf);

		auto r = i64(m_regs.color_value.vec[0]);
		auto g = i64(m_regs.color_value.vec[1]);
		auto b = i64(m_regs.color_value.vec[2]);

		auto fc = m_regs.far_color_vec;

		auto prev_mac1 = SetMac<1>(CheckMacOverflow<1>((r * m_regs.ir1.vec[0]) << 4));
		auto prev_mac2 = SetMac<2>(CheckMacOverflow<2>((g * m_regs.ir2.vec[0]) << 4));
		auto prev_mac3 = SetMac<3>(CheckMacOverflow<3>((b * m_regs.ir3.vec[0]) << 4));

		SaturateIR<1>(i32(SetMac<1>(prev_mac1 >> (12 * sf))), lm);
		SaturateIR<2>(i32(SetMac<2>(prev_mac2 >> (12 * sf))), lm);
		SaturateIR<3>(i32(SetMac<3>(prev_mac3 >> (12 * sf))), lm);

		PushColorFromMac();
		UpdateIRGB();
	}

	void GTE::NCS(GTE_CmdEncoding cmd, u8 vec_number) {
		auto sf = u32(cmd.sf());
		auto lm = bool(cmd.lm());

		auto llm = m_regs.light_matrix;
		auto bk = m_regs.bg_color_vec;
		auto lcm = m_regs.light_color_mat;

		GTE_Vec3<i16> vx{};

		switch (vec_number)
		{
		case 0:
			vx = m_regs.v0;
			break;
		case 1:
			vx = m_regs.v1;
			break;
		case 2:
			vx = m_regs.v2;
			break;
		default:
			error::Unreachable();
			break;
		}

		VectorMatMul(llm, vx, {}, lm, sf);
		VectorMatMul(lcm, IRToVec(), bk, lm, sf);

		PushColorFromMac();
		UpdateIRGB();
	}

	void GTE::NCT(GTE_CmdEncoding cmd) {
		NCS(cmd, 0);
		NCS(cmd, 1);
		NCS(cmd, 2);
	}

	void GTE::SQR(GTE_CmdEncoding cmd) {
		auto sf = u32(cmd.sf());
		VectorMul(IRToVec(), IRToVec(), {}, false, sf);
	}

	void GTE::DPCL(GTE_CmdEncoding cmd) {
		auto sf = u32(cmd.sf());
		auto lm = bool(cmd.lm());

		auto r = i64(m_regs.color_value.vec[0]);
		auto g = i64(m_regs.color_value.vec[1]);
		auto b = i64(m_regs.color_value.vec[2]);

		auto fc = m_regs.far_color_vec;

		auto prev_mac1 = SetMac<1>(CheckMacOverflow<1>((r * m_regs.ir1.vec[0]) << 4));
		auto prev_mac2 = SetMac<2>(CheckMacOverflow<2>((g * m_regs.ir2.vec[0]) << 4));
		auto prev_mac3 = SetMac<3>(CheckMacOverflow<3>((b * m_regs.ir3.vec[0]) << 4));

		SaturateIR<1>(i32(SetMac<1>(CheckMacOverflow<1>((i64(fc.vec[0]) << 12) - prev_mac1) >> (12 * sf))), false);
		SaturateIR<2>(i32(SetMac<2>(CheckMacOverflow<2>((i64(fc.vec[1]) << 12) - prev_mac2) >> (12 * sf))), false);
		SaturateIR<3>(i32(SetMac<3>(CheckMacOverflow<3>((i64(fc.vec[2]) << 12) - prev_mac3) >> (12 * sf))), false);

		SaturateIR<1>(i32(SetMac<1>(CheckMacOverflow<1>(prev_mac1 + i64(m_regs.ir1.vec[0]) * m_regs.ir0.vec[0]) >> (12 * sf))), lm);
		SaturateIR<2>(i32(SetMac<2>(CheckMacOverflow<2>(prev_mac2 + i64(m_regs.ir2.vec[0]) * m_regs.ir0.vec[0]) >> (12 * sf))), lm);
		SaturateIR<3>(i32(SetMac<3>(CheckMacOverflow<3>(prev_mac3 + i64(m_regs.ir3.vec[0]) * m_regs.ir0.vec[0]) >> (12 * sf))), lm);

		PushColorFromMac();
		UpdateIRGB();
	}

	void GTE::DPCT(GTE_CmdEncoding cmd) {
		DPCS(cmd, true);
		DPCS(cmd, true);
		DPCS(cmd, true);
	}

	void GTE::AVSZ3(GTE_CmdEncoding cmd) {
		auto mac0 = SetMac0(i64(m_regs.avg_z_scale_0.vec[0]) * (i64(m_regs.sz1.vec[0]) + m_regs.sz2.vec[0] + m_regs.sz3.vec[0]));
		SaturateOTZ(mac0 >> 12);
	}

	void GTE::AVSZ4(GTE_CmdEncoding cmd) {
		auto mac0 = SetMac0(i64(m_regs.avg_z_scale_1.vec[0]) * (i64(m_regs.sz0.vec[0]) + m_regs.sz1.vec[0] + m_regs.sz2.vec[0] + m_regs.sz3.vec[0]));
		SaturateOTZ(mac0 >> 12);
	}

	void GTE::RTPT(GTE_CmdEncoding cmd) {
		RTPS(cmd, 0, false);
		RTPS(cmd, 1, false);
		RTPS(cmd, 2, true);
	}

	void GTE::GPF(GTE_CmdEncoding cmd) {
		Interpolate(0, 0, 0, bool(cmd.lm()), u32(cmd.sf()));
		PushColorFromMac();
		UpdateIRGB();
	}

	void GTE::GPL(GTE_CmdEncoding cmd) {
		u32 sf = u32(cmd.sf());
		auto mac1 = CheckMacOverflow<1>(i64(m_regs.mac1_3.vec[0]) << (12 * sf));
		auto mac2 = CheckMacOverflow<2>(i64(m_regs.mac1_3.vec[1]) << (12 * sf));
		auto mac3 = CheckMacOverflow<3>(i64(m_regs.mac1_3.vec[2]) << (12 * sf));
		Interpolate(mac1, mac2, mac3, bool(cmd.lm()), sf);
		PushColorFromMac();
		UpdateIRGB();
	}

	void GTE::NCCT(GTE_CmdEncoding cmd) {
		NCCS(cmd, 0);
		NCCS(cmd, 1);
		NCCS(cmd, 2);
	}

	void GTE::VectorMul(GTE_Vec3<i16> v1, GTE_Vec3<i16> v2, GTE_Vec3<i16> tr, bool lm, u32 sf) {
		SaturateIR<1>(i32(SetMac<1>(CheckMacOverflow<1>((i64(tr.vec[0]) << 12) + i64(v1.vec[0]) * v2.vec[0]) >> (12 * sf))) , lm);
		SaturateIR<2>(i32(SetMac<2>(CheckMacOverflow<2>((i64(tr.vec[1]) << 12) + i64(v1.vec[1]) * v2.vec[1]) >> (12 * sf))) , lm);
		SaturateIR<3>(i32(SetMac<3>(CheckMacOverflow<3>((i64(tr.vec[2]) << 12) + i64(v1.vec[2]) * v2.vec[2]) >> (12 * sf))) , lm);
	}

	void GTE::VectorMatMul(GTE_Matrix mx, GTE_Vec3<i16> vx, GTE_Vec3<i32> tx, bool lm, u32 sf) {
		auto temp1 = CheckMacOverflow<1>(CheckMacOverflow<1>(CheckMacOverflow<1>((i64(tx.vec[0]) << 12) + i64(mx.mat[0][0]) * vx.vec[0]) + i64(mx.mat[0][1]) * vx.vec[1]) + i64(mx.mat[0][2]) * vx.vec[2]);
		auto temp2 = CheckMacOverflow<2>(CheckMacOverflow<2>(CheckMacOverflow<2>((i64(tx.vec[1]) << 12) + i64(mx.mat[1][0]) * vx.vec[0]) + i64(mx.mat[1][1]) * vx.vec[1]) + i64(mx.mat[1][2]) * vx.vec[2]);
		auto temp3 = CheckMacOverflow<3>(CheckMacOverflow<3>(CheckMacOverflow<3>((i64(tx.vec[2]) << 12) + i64(mx.mat[2][0]) * vx.vec[0]) + i64(mx.mat[2][1]) * vx.vec[1]) + i64(mx.mat[2][2]) * vx.vec[2]);

		SaturateIR<1>(i32(SetMac<1>(temp1 >> (12 * sf))), lm);
		SaturateIR<2>(i32(SetMac<2>(temp2 >> (12 * sf))), lm);
		SaturateIR<3>(i32(SetMac<3>(temp3 >> (12 * sf))), lm);
	}

	void GTE::Interpolate(i64 mac1, i64 mac2, i64 mac3, bool lm, u32 sf) {
		auto temp1 = CheckMacOverflow<1>(i64(m_regs.ir1.vec[0]) * m_regs.ir0.vec[0] + mac1);
		auto temp2 = CheckMacOverflow<2>(i64(m_regs.ir2.vec[0]) * m_regs.ir0.vec[0] + mac2);
		auto temp3 = CheckMacOverflow<3>(i64(m_regs.ir3.vec[0]) * m_regs.ir0.vec[0] + mac3);

		SaturateIR<1>(i32(SetMac<1>(temp1 >> (12 * sf))), lm);
		SaturateIR<2>(i32(SetMac<2>(temp2 >> (12 * sf))), lm);
		SaturateIR<3>(i32(SetMac<3>(temp3 >> (12 * sf))), lm);
	}

	void GTE::UpdateIRGB() {
		auto clamp = [](auto value, auto min, auto max) {
			if (value < min)
				return min;
			if (value > max)
				return max;
			return value;
		};

		RGB_555 color{};

		color.set_red(clamp(m_regs.ir1.vec[0] / 0x80, 0x0, 0x1F));
		color.set_green(clamp(m_regs.ir2.vec[0] / 0x80, 0x0, 0x1F));
		color.set_blue(clamp(m_regs.ir3.vec[0] / 0x80, 0x0, 0x1F));

		m_regs.irgb.vec[0] = u16(color);
		m_regs.irgb.vec[1] = u16(color);
	}

}