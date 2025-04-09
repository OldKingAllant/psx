#include <psxemu/include/psxemu/GTE.hpp>
#include <psxemu/include/psxemu/SystemStatus.hpp>

namespace psx::cpu {
#pragma optimize("", off)
	void GTE::RTPS(GTE_CmdEncoding cmd) {
		//IR1 = MAC1 = (TRX * 1000h + RT11 * VX0 + RT12 * VY0 + RT13 * VZ0) SAR(sf * 12)
		//IR2 = MAC2 = (TRY * 1000h + RT21 * VX0 + RT22 * VY0 + RT23 * VZ0) SAR(sf * 12)
		//IR3 = MAC3 = (TRZ * 1000h + RT31 * VX0 + RT32 * VY0 + RT33 * VZ0) SAR(sf * 12)
		//SZ3 = MAC3 SAR((1 - sf) * 12); ScreenZ FIFO 0.. + FFFFh
		//MAC0 = (((H * 20000h / SZ3) + 1) / 2) * IR1 + OFX, SX2 = MAC0 / 10000h; ScrX FIFO - 400h.. + 3FFh
		//MAC0 = (((H * 20000h / SZ3) + 1) / 2) * IR2 + OFY, SY2 = MAC0 / 10000h; ScrY FIFO - 400h.. + 3FFh
		//MAC0 = (((H * 20000h / SZ3) + 1) / 2) * DQA + DQB, IR0 = MAC0 / 1000h; Depth cueing 0.. + 1000h
		auto sf = u32(cmd.sf());
		auto lm = bool(cmd.lm());

		auto tr = m_regs.translation_vec;
		auto rt = m_regs.rotation_matrix;
		auto v0 = m_regs.v0;

		auto temp1 = CheckMacOverflow<1>(CheckMacOverflow<1>(CheckMacOverflow<1>(i64(tr.vec[0]) * 0x1000 + i64(rt.mat[0][0]) * v0.vec[0]) + i64(rt.mat[0][1]) * v0.vec[1]) + i64(rt.mat[0][2]) * v0.vec[2]);
		auto ir1 = SaturateIR<1>(SetMac<1>(temp1 >> (sf * 12)), lm);
		auto temp2 = CheckMacOverflow<2>(CheckMacOverflow<2>(CheckMacOverflow<2>(i64(tr.vec[1]) * 0x1000 + i64(rt.mat[1][0]) * v0.vec[0]) + i64(rt.mat[1][1]) * v0.vec[1]) + i64(rt.mat[1][2]) * v0.vec[2]);
		auto ir2 = SaturateIR<2>(SetMac<2>(temp2 >> (sf * 12)), lm);
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

		RGB_555 color{};

		color.set_red(clamp(m_regs.ir1.vec[0] / 0x80, 0x0, 0x1F));
		color.set_green(clamp(m_regs.ir2.vec[0] / 0x80, 0x0, 0x1F));
		color.set_blue(clamp(m_regs.ir3.vec[0] / 0x80, 0x0, 0x1F));

		m_regs.irgb.vec[0] = u16(color);
		m_regs.irgb.vec[1] = u16(color);

		auto sz3 = SaturateSZ3(temp3 >> 12);
		auto h = m_regs.proj_plane_dist.vec[0];

		auto ofx = m_regs.screen_offset.vec[0];
		auto ofy = m_regs.screen_offset.vec[1];

		auto dqa = m_regs.dqa;
		auto dqb = m_regs.dqb;

		auto div_result = i64(DivAndSaturate(h, sz3));

		auto mac0 = int64_t(0);

		mac0 = SetMac0(div_result * ir1 + ofx);
		auto sx2 = SaturateSX2(mac0 >> 16);
		mac0 = SetMac0(div_result * ir2 + ofy);
		auto sy2 = SaturateSY2(mac0 >> 16);
		mac0 = SetMac0(div_result * dqa + dqb);
		SaturateIR0(mac0 >> 12);

		GTE_Vec2<i16> screen_xy{};
		screen_xy.vec[0] = sx2; screen_xy.vec[1] = sy2;
		PushScreenFifo(screen_xy);
		PushZFifo(sz3);

		m_cmd_interlock = m_sys_status->scheduler.GetTimestamp() + 15;
	}
#pragma optimize("", on)
}