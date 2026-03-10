#include <psxemu/include/psxemu/SPU.hpp>

#include <utility>
#include <algorithm>

namespace psx {
#define W(value, offset) (ReverbWrite((i16)std::clamp<i32>(value, -0x8000, 0x7FFF), offset))
#define R(offset) (ReverbRead(offset))
#define MULT(l, r) (((l) * (r)) >> 15)

#pragma optimize("", off)
	std::pair<i32, i32> SPU::DoReverb(i32 l, i32 r) {
		bool old_reverb_cycle = m_reverb_odd_cycle;
		m_reverb_odd_cycle = !m_reverb_odd_cycle;
		if (old_reverb_cycle) {
			return { 0, 0 };
		}

		const i32 vlout = m_regs.m_reverb.vol_left;
		const i32 vrout = m_regs.m_reverb.vol_right;
		const u32 mbase = u32(m_regs.m_reverb.work_area_start) << 3;
		const u32 dapf1 = (u32)m_regs.m_reverb.apf_off_1 << 3;
		const u32 dapf2 = (u32)m_regs.m_reverb.apf_off_2 << 3;
		const i32 viir = m_regs.m_reverb.refl_vol_1;
		const i32 vcomb1 = m_regs.m_reverb.comb_vol_1;
		const i32 vcomb2 = m_regs.m_reverb.comb_vol_2;
		const i32 vcomb3 = m_regs.m_reverb.comb_vol_3;
		const i32 vcomb4 = m_regs.m_reverb.comb_vol_4;
		const i32 vwall = m_regs.m_reverb.refl_vol_2;
		const i32 vapf1 = m_regs.m_reverb.apf_vol_1;
		const i32 vapf2 = m_regs.m_reverb.apf_vol_2;
		const u32 mlsame = (u32)m_regs.m_reverb.same_side_refl_add_1_left << 3;
		const u32 mrsame = (u32)m_regs.m_reverb.same_side_refl_add_1_right << 3;
		const u32 mlcomb1 = (u32)m_regs.m_reverb.comb_add_1_left << 3;
		const u32 mrcomb1 = (u32)m_regs.m_reverb.comb_add_1_right << 3;
		const u32 mlcomb2 = (u32)m_regs.m_reverb.comb_add_2_left << 3;
		const u32 mrcomb2 = (u32)m_regs.m_reverb.comb_add_2_right << 3;
		const u32 dlsame = (u32)m_regs.m_reverb.same_side_refl_add_2_left << 3;
		const u32 drsame = (u32)m_regs.m_reverb.same_side_refl_add_2_right << 3;
		const u32 mldiff = (u32)m_regs.m_reverb.diff_side_refl_add_1_left << 3;
		const u32 mrdiff = (u32)m_regs.m_reverb.diff_side_refl_add_1_right << 3;
		const u32 mlcomb3 = (u32)m_regs.m_reverb.comb_add_3_left << 3;
		const u32 mrcomb3 = (u32)m_regs.m_reverb.comb_add_3_right << 3;
		const u32 mlcomb4 = (u32)m_regs.m_reverb.comb_add_4_left << 3;
		const u32 mrcomb4 = (u32)m_regs.m_reverb.comb_add_4_right << 3;
		const u32 dldiff = (u32)m_regs.m_reverb.diff_side_refl_add_2_left << 3;
		const u32 drdiff = (u32)m_regs.m_reverb.diff_side_refl_add_2_right << 3;
		const u32 mlapf1 = (u32)m_regs.m_reverb.apf_add_1_left << 3;
		const u32 mrapf1 = (u32)m_regs.m_reverb.apf_add_1_right << 3;
		const u32 mlapf2 = (u32)m_regs.m_reverb.apf_add_2_left << 3;
		const u32 mrapf2 = (u32)m_regs.m_reverb.apf_add_2_right << 3;
		const i32 vlin = m_regs.m_reverb.input_vol_left;
		const i32 vrin = m_regs.m_reverb.input_vol_right;

		//Do reverb
		i32 lin = MULT(vlin, l);
		i32 rin = MULT(vrin, r);

		if(m_regs.m_cnt.reverb_master_en) {
			W(MULT(lin + MULT(R(dlsame), vwall) - R(mlsame - 2), viir) + R(mlsame - 2), mlsame);
			W(MULT(rin + MULT(R(drsame), vwall) - R(mrsame - 2), viir) + R(mrsame - 2), mrsame);

			W(MULT(lin + MULT(R(drdiff), vwall) - R(mldiff - 2), viir) + R(mldiff - 2), mldiff);
			W(MULT(rin + MULT(R(dldiff), vwall) - R(mrdiff - 2), viir) + R(mrdiff - 2), mrdiff);
		}

		i32 lout{}, rout{};

		lout = MULT(vcomb1, R(mlcomb1)) + MULT(vcomb2, R(mlcomb2)) + MULT(vcomb3, R(mlcomb3)) + MULT(vcomb4, R(mlcomb4));
		rout = MULT(vcomb1, R(mrcomb1)) + MULT(vcomb2, R(mrcomb2)) + MULT(vcomb3, R(mrcomb3)) + MULT(vcomb4, R(mrcomb4));

		lout = lout - MULT(vapf1, R(mlapf1 - dapf1));
		rout = rout - MULT(vapf1, R(mrapf1 - dapf1));

		W(lout, mlapf1);
		W(rout, mrapf1);

		lout = MULT(lout, vapf1) + R(mlapf1 - dapf1);
		rout = MULT(rout, vapf1) + R(mrapf1 - dapf1);

		/////////////////////

		lout = lout - MULT(vapf2, R(mlapf2 - dapf2));
		rout = rout - MULT(vapf2, R(mrapf2 - dapf2));

		W(lout, mlapf2);
		W(rout, mrapf2);

		lout = MULT(lout, vapf2) + R(mlapf2 - dapf2);
		rout = MULT(rout, vapf2) + R(mrapf2 - dapf2);

		lout = MULT(lout, vlout);
		rout = MULT(rout, vrout);

		m_reverb_buf_address = std::max(mbase, (m_reverb_buf_address + 2) & REVERB_BUFFER_END);

		//m_fir_left.Push(lout);
		//m_fir_right.Push(rout);
		//
		//lout = m_fir_left.Apply();
		//rout = m_fir_right.Apply();

		return { lout, rout };
	}

	void SPU::ReverbWrite(i16 value, i32 offset) {
		if (!m_regs.m_cnt.reverb_master_en) {
			return;
		}
		u32 final_address = u32(m_reverb_buf_address + offset);
		const u32 MBASE = (u32)m_regs.m_reverb.work_area_start << 3;
		final_address = std::max<u32>(MBASE, final_address & REVERB_BUFFER_END);
		WriteRamDirect16((u16)value, final_address);
	}

	i16 SPU::ReverbRead(i32 offset) {
		u32 final_address = u32(m_reverb_buf_address + offset);
		const u32 MBASE = (u32)m_regs.m_reverb.work_area_start << 3;
		final_address = std::max<u32>(MBASE, final_address & REVERB_BUFFER_END);
		return ReadRamDirect16(final_address).first;
	}
#pragma optimize("", on)
}