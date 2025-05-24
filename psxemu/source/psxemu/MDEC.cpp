#include <psxemu/include/psxemu/MDEC.hpp>
#include <psxemu/include/psxemu/SystemStatus.hpp>
#include <psxemu/include/psxemu/SystemBus.hpp>

#include <psxemu/include/psxemu/Logger.hpp>
#include <psxemu/include/psxemu/LoggerMacros.hpp>

#include <common/Errors.hpp>

namespace psx {
	MDEC::MDEC(system_status* sys_status) :
		m_stat{},
		m_enable_dma0{false},
		m_enable_dma1{false},
		m_num_params{},
		m_in_fifo{},
		m_luminance_table{},
		m_color_table{},
		m_scale_table{},
		m_curr_cmd{},
		m_sys_status{sys_status},
		m_can_use_fast_idct{false},
		m_out_fifo{},
		m_curr_in_pos{},
		m_curr_out_pos{}
	{}

	void MDEC::WriteCommand(u32 value) {
		if (m_num_params > 0) {
			m_in_fifo[m_curr_in_pos++] = u16(value);
			m_in_fifo[m_curr_in_pos++] = u16(value >> 16);

			m_num_params -= 1;

			m_stat.data_in_full = m_num_params == 0;

			if (m_stat.data_in_full) {
				switch (m_curr_cmd)
				{
				case MDEC_Cmd::DECODE:
					Decode();
					break;
				case MDEC_Cmd::SET_QUANT:
					FillLuminance();
					break;
				case MDEC_Cmd::SET_SCALE:
					FillScale();
					break;
				default:
					error::Unreachable();
					break;
				}

				m_stat.data_in_full = false;
				m_stat.cmd_busy = m_num_params > 0;
				m_curr_cmd = MDEC_Cmd::IDLE;
			}
			else {
				m_stat.missing_params = m_num_params - 1;
			}
			
			UpdateDMA0Req();
		}
		else {
			CmdStart(value);
		}
	}

	void MDEC::WriteControl(u32 value) {
		if ((value >> 31) & 1) {
			LOG_DEBUG("MDEC", "[MDEC] RESET");
			Reset();
		}

		if ((value >> 30) & 1) {
			LOG_DEBUG("MDEC", "[MDEC] Enable Data-In");
			m_enable_dma0 = true;
		}

		if ((value >> 29) & 1) {
			LOG_DEBUG("MDEC", "[MDEC] Enable Data-Out");
			m_enable_dma1 = true;
		}

		UpdateDMA0Req();
		UpdateDMA1Req();
	}

	u32 MDEC::ReadData() {
		if (m_out_fifo.empty() || m_curr_out_pos == m_out_fifo.size()) {
			return 0;
		}
		auto value = m_out_fifo[m_curr_out_pos++];

		UpdateDMA0Req();
		UpdateDMA1Req();
		return value;
	}

	u32 MDEC::ReadStat() {
		m_stat.data_out_empty = m_out_fifo.empty() ||
			m_curr_out_pos == m_out_fifo.size();
		return m_stat.raw;
	}

	void MDEC::Reset() {
		m_stat.raw = STAT_RESET_VALUE;
		m_num_params = 0;
		m_in_fifo.clear();
		m_out_fifo.clear();
		m_enable_dma0 = false;
		m_enable_dma1 = false;
		m_curr_in_pos = 0;
		m_curr_out_pos = 0;
		UpdateDMA0Req();
		UpdateDMA1Req();
	}

	void MDEC::CmdStart(u32 value) {
		auto cmd = MDEC_Cmd((value >> 29) & 0x7);

		m_stat.raw &= ~(0xF << 23);
		auto set_bits = (value >> 25) & 0xF;
		m_stat.raw |= set_bits << 23;

		switch (cmd)
		{
		case psx::MDEC_Cmd::DECODE: {
			m_curr_cmd = MDEC_Cmd::DECODE;
			m_stat.cmd_busy = true;
			m_num_params = u16(value & 0xFFFF);
			m_stat.missing_params = m_num_params - 1;
			m_stat.curr_block = CurrBlockType::CR;
			m_out_fifo.clear();
			m_curr_out_pos = 0;

			const char* color_depth_repr = "INVALID";

			switch (m_stat.out_depth)
			{
			case OutputDepth::BIT4:
				color_depth_repr = "4BPP";
				break;
			case OutputDepth::BIT8:
				color_depth_repr = "8BPP";
				break;
			case OutputDepth::BIT15:
				color_depth_repr = "15BPP";
				break;
			case OutputDepth::BIT24:
				color_depth_repr = "24BPP";
				break;
			default:
				error::Unreachable();
				break;
			}

			LOG_DEBUG("MDEC", "[MDEC] DECODE MACROBLOCKS (bytes={:#x}, signed={}, set_mask={}, out_depth={})", 
				(m_num_params - 1) * 4, bool(m_stat.data_out_signed), 
				bool(m_stat.data_out_bit15), color_depth_repr);
		}
		break;
		case psx::MDEC_Cmd::SET_QUANT: {
			bool set_color = bool(value & 1);
			LOG_DEBUG("MDEC", "[MDEC] SET QUANT (color = {})", 
				set_color);
			m_num_params = (64 + (64 * set_color)) / 4;
			m_stat.missing_params = m_num_params - 1;
			m_curr_cmd = MDEC_Cmd::SET_QUANT;
			m_stat.cmd_busy = true;
		}
		break;
		case psx::MDEC_Cmd::SET_SCALE: {
			LOG_DEBUG("MDEC", "[MDEC] SET SCALE");
			m_curr_cmd = MDEC_Cmd::SET_SCALE;
			m_num_params = 32; //64 signed shorts
			m_stat.missing_params = m_num_params - 1;
			m_stat.cmd_busy = true;
		}
		break;
		default: {
			LOG_WARN("MDEC", "[MDEC] Invalid command {:#x}",
				u32(cmd));
			m_stat.missing_params = u16(value & 0xFFFF);
		}
		break;
		}

		if (m_in_fifo.size() < std::size_t(m_num_params) * 2) {
			m_in_fifo.resize(std::size_t(m_num_params) * 2);
		}
		m_curr_in_pos = 0;

		UpdateDMA0Req();
	}

	void MDEC::FillLuminance() {
		for (std::size_t index = 0; index < 64; index += 2) {
			auto curr_entry = m_in_fifo[index >> 1];
			m_luminance_table[index + 0] = u8(curr_entry >> 0);
			m_luminance_table[index + 1] = u8(curr_entry >> 8);
		}

		if (m_curr_in_pos > 32) {
			FillColor(32);
		}
	}

	void MDEC::FillColor(std::size_t index_base) {
		for (std::size_t index = 0; index < 64; index += 2) {
			auto curr_entry = m_in_fifo[index_base + (index >> 1)];
			m_color_table[index + 0] = u8(curr_entry >> 0);
			m_color_table[index + 1] = u8(curr_entry >> 8);
		}
	}

	void MDEC::FillScale() {
		for (std::size_t y = 0; y < 8; y++) {
			for (std::size_t x = 0; x < 8; x++) {
				m_scale_table[y * 8 + x] = i16(m_in_fifo[x * 8 + y]);
			}
		}

		m_can_use_fast_idct = std::memcmp(DEFAULT_SCALE_TABLE.data(),
			m_scale_table.data(), m_scale_table.size() * 
			sizeof(decltype(m_scale_table)::value_type)) == 0;
	}

	void MDEC::Decode() {
		if (m_stat.out_depth == OutputDepth::BIT4 ||
			m_stat.out_depth == OutputDepth::BIT8) {
			auto in_iter = m_in_fifo.cbegin();
			auto end_iter = in_iter + m_curr_in_pos;

			while (in_iter != end_iter) {
				auto decoded = DecodeSingleBlock(in_iter, m_luminance_table);

				if (!decoded.has_value()) {
					continue;
				}

				//Decode single block
				auto block = YToMono(decoded.value());
				//Push to output fifo
				VecToOutFifo(block);
				//Start DMA
				UpdateDMA1Req();
				m_stat.curr_block = CurrBlockType(4);
			}
		}
		else {
			auto in_iter = m_in_fifo.cbegin();
			auto end_iter = in_iter + m_curr_in_pos;

			while (in_iter != end_iter) {
				auto cr = DecodeSingleBlock(in_iter, m_color_table);
				auto cb = DecodeSingleBlock(in_iter, m_color_table);

				auto y1 = DecodeSingleBlock(in_iter, m_luminance_table);
				auto y2 = DecodeSingleBlock(in_iter, m_luminance_table);
				auto y3 = DecodeSingleBlock(in_iter, m_luminance_table);
				auto y4 = DecodeSingleBlock(in_iter, m_luminance_table);

				if (!cr || !cb || !y1 || !y2 || !y3 || !y4) {
					continue;
				}

				std::array<u32, 256> macroblock{};

				YUVToRGB(y1.value(), cr.value(), cb.value(), macroblock, 0, 0);
				YUVToRGB(y2.value(), cr.value(), cb.value(), macroblock, 8, 0);
				YUVToRGB(y3.value(), cr.value(), cb.value(), macroblock, 0, 8);
				YUVToRGB(y4.value(), cr.value(), cb.value(), macroblock, 8, 8);

				VecToOutFifo(macroblock);
				UpdateDMA1Req();
			}
		}
	}

	void MDEC::UpdateDMA0Req() {
		m_stat.data_in_request = m_enable_dma0;

		m_sys_status->sysbus->GetDMAControl()
			.GetMDECInDma().SetDreq(m_stat.data_in_request);

		if (m_stat.data_in_request) {
			m_sys_status->sysbus->GetDMAControl()
				.GetMDECInDma().DreqRisingEdge();
		}
	}

	void MDEC::UpdateDMA1Req() {
		m_stat.data_out_request = m_enable_dma1 && 
			m_curr_out_pos < m_out_fifo.size();

		m_sys_status->sysbus->GetDMAControl()
			.GetMDECOutDma().SetDreq(m_stat.data_out_request);

		if (m_stat.data_out_request) {
			m_sys_status->sysbus->GetDMAControl()
				.GetMDECOutDma().DreqRisingEdge();
		}
	}

	using DecodedBlock = MDEC::DecodedBlock;
	using block_iter = MDEC::block_iter;

	std::optional<DecodedBlock> MDEC::DecodeSingleBlock(block_iter& iter, std::array<u8, 64> const& qt) {
		auto end_iter = m_in_fifo.cbegin() + m_curr_in_pos;

		if (iter == end_iter) {
			return std::nullopt;
		}

		DecodedBlock block{};
		block.fill(0);

		while (iter != end_iter && *iter == 0xFE00) {
			iter++;
		}

		if (iter == end_iter) {
			return std::nullopt;
		}

		std::size_t idx = 0;
		u16 k = 0; 
		u16 n = *iter++;

		//First halfword contains
		//quant scale value and DC (direct current)
		//value
		u16 qscale = (n >> 10) & 0x3F;
		auto val = sign_extend<i32, 9>(n & 0x3FF) * qt[k];

		while(k < 63) {
			if (qscale == 0) {
				val = sign_extend<i32, 9>(n & 0x3FF) * 2;
			}

			val = std::clamp(i32(val), -0x400, 0x3FF);
			//val = val*scalezag[i]

			if (qscale > 0) {
				block[ZAGZIG[k]] = i16(val);
			}
			else if(qscale == 0) {
				block[k] = i16(val);
			}

			if (iter == end_iter) {
				return std::nullopt;
			}
			n = *iter++;
			//From the current RL-compressed
			//entry, get count of halfwords
			//set to zero, sum it to current
			//block size
			k = k + ((n >> 10) & 0x3F) + 1;
			val = (sign_extend<i32, 9>(n & 0x3FF) * qt[k] * qscale + 4) / 8;
		}	

		/*
		if (iter == end_iter && k < 63) {
			return std::nullopt;
		}

		if (k < 64)
		{
			if (qscale == 0) {
				val = sign_extend<i32, 9>(n & 0x3FF) * 2;
			}

			val = std::clamp(i32(val), -0x400, 0x3FF);

			if (qscale > 0) {
				block[ZAGZIG[k]] = i16(val);
			}
			else if (qscale == 0) {
				block[k] = i16(val);
			}
		}*/

		IdctCore(block);
		return block;
	}

	std::array<u32, 256> MDEC::YToMono(DecodedBlock& block) const {
		std::array<u32, 256> rgb_block{};

		for (std::size_t idx = 0; idx < 64; idx++) {
			i32 curr_val     = sign_extend<i32, 8>(block[idx]);
			curr_val     = i32(std::clamp(i32(curr_val), -128, 127));
			if (!m_stat.data_out_signed) {
				curr_val += 0x80;
			}
			rgb_block[idx] = u32(curr_val);
		}

		return rgb_block;
	}

	void MDEC::YUVToRGB(DecodedBlock& block, DecodedBlock& cr, DecodedBlock& cb, std::array<u32, 256>& out, std::size_t xx, std::size_t yy) const {
		for (std::size_t y = 0; y < 8; y++) {
			for (std::size_t x = 0; x < 8; x++) {
				auto cr_sample = cr[((x + xx) / 2) + ((y + yy) / 2) * 8];
				auto cb_sample = cb[((x + xx) / 2) + ((y + yy) / 2) * 8];
				auto Y = block[x + y * 8];

				auto R = i16(1.402f * float(cr_sample));
				auto B = i16(1.772f * float(cb_sample));
				auto G = i16(-(0.3437f * float(cb_sample)) - (0.7143f * float(cr_sample)));
				
				R = i16(std::clamp(Y+R, -127, 128));
				G = i16(std::clamp(Y+G, -127, 128));
				B = i16(std::clamp(Y+B, -127, 128));
				if (!m_stat.data_out_signed) {
					R += 0x80;
					G += 0x80;
					B += 0x80;
				}
				u32 color{};
				color |= (u32(R) << 0);
				color |= (u32(G) << 8);
				color |= (u32(B) << 16);
				out[(x + xx) + (y + yy) * 16] = color;
			}
		}
	}
	
	void MDEC::VecToOutFifo(std::array<u32, 256> const& src) {
		switch (m_stat.out_depth)
		{
		case OutputDepth::BIT4: {
			for (std::size_t idx = 0; idx < 64; idx += 8) {
				u32 curr_value = src[idx] >> 4;
				curr_value |= (src[idx + 1] >> 4) << 4;
				curr_value |= (src[idx + 2] >> 4) << 8;
				curr_value |= (src[idx + 3] >> 4) << 12;
				curr_value |= (src[idx + 4] >> 4) << 16;
				curr_value |= (src[idx + 5] >> 4) << 20;
				curr_value |= (src[idx + 6] >> 4) << 24;
				curr_value |= (src[idx + 7] >> 4) << 28;
				m_out_fifo.push_back(curr_value);
			}
		}
			break;
		case OutputDepth::BIT8: {
			for (std::size_t idx = 0; idx < 64; idx += 4) {
				u32 curr_value = src[idx];
				curr_value |= (src[idx + 1] << 8);
				curr_value |= (src[idx + 2] << 16);
				curr_value |= (src[idx + 3] << 24);
				m_out_fifo.push_back(curr_value);
			}
		}
			break;
		case OutputDepth::BIT15: {
			for (std::size_t idx = 0; idx < 256; idx += 2) {
				u32 curr_value{};
				u32 color1 = src[idx];
				u32 color2 = src[idx + 1];

				{
					auto r = u16((color1 >> 3) & 0x1F);
					auto g = u16((color1 >> 11) & 0x1F);
					auto b = u16((color1 >> 19) & 0x1F);
					color1 = r | (g << 5) | (b << 10);
				}

				{
					auto r = u16((color2 >> 3) & 0x1F);
					auto g = u16((color2 >> 11) & 0x1F);
					auto b = u16((color2 >> 19) & 0x1F);
					color2 = r | (g << 5) | (b << 10);
				}

				if (m_stat.data_out_bit15) {
					color1 |= (1 << 15);
					color2 |= (1 << 15);
				}

				curr_value |= color1;
				curr_value |= (color2 << 16);
				m_out_fifo.push_back(curr_value);
			}
		}
			break;
		case OutputDepth::BIT24: {
			//Colors are packed with no spaces in between:
			//1. RGBR
			//2. GBRG
			//3. BRGB
			//4. Repeat
			auto state = 0;
			u32 color = 0;

			for (std::size_t idx = 0; idx < 256; idx++) {
				switch (state)
				{
				case 0: {
					//color = RGB-
					color = src[idx];
					state = 1;
				}
					break;
				case 1: {
					//temp = RGB
					u32 temp = src[idx];
					//color = RGBR
					color |= ((temp & 0xFF) << 24);
					m_out_fifo.push_back(color);
					//color = GB--
					color = (temp >> 8);
					state = 2;
				}
					break;
				case 2: {
					u32 temp = src[idx];
					//color = GBRG
					color |= ((temp & 0xFFFF) << 16);
					m_out_fifo.push_back(color);
					//color = B---
					color = (temp >> 16);
					state = 3;
				}
					break;
				case 3: {
					u32 temp = src[idx];
					//color = BRGB
					color |= ((temp & 0xFFFFFF) << 8);
					m_out_fifo.push_back(color);

					//No need to set color again, full color
					//data from current index has been used
					//and state is reset
					state = 0;
				}
					break;
				default:
					error::Unreachable();
					break;
				}
			}
		}
			break;
		default:
			error::Unreachable();
			break;
		}
	}

	void MDEC::IdctCore(std::array<i16, 64>& blk) {
		std::array<i64, 64> temp{};

		for (std::size_t x = 0; x < 8; x++) {
			for (std::size_t y = 0; y < 8; y++) {
				i64 sum = 0;
				for (std::size_t z = 0; z < 8; z++) {
					sum += i64(blk[x+z*8] * m_scale_table[y*8+z]);
				}
				temp[x+y*8] = sum;
			}
		}

		//Repeat with src/dst exchanged
		for (std::size_t x = 0; x < 8; x++) {
			for (std::size_t y = 0; y < 8; y++) {
				i64 sum = 0;
				for (std::size_t z = 0; z < 8; z++) {
					sum += i64(temp[y*8+z] * m_scale_table[x*8+z]);
				}

				int round = (sum >> 31) & 1;
				blk[x + y * 8] = i16(std::clamp<i32>(
					sign_extend<i32, 8>((sum >> 32) + round), 
					-128, 127
				));
			}
		}
	}

}