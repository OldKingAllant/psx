#include <psxemu/include/psxemu/MDEC.hpp>
#include <psxemu/include/psxemu/SystemStatus.hpp>
#include <psxemu/include/psxemu/SystemBus.hpp>

#include <psxemu/include/psxemu/Logger.hpp>
#include <psxemu/include/psxemu/LoggerMacros.hpp>

#include <common/Errors.hpp>

#include <immintrin.h>
#include <xmmintrin.h>
#include <emmintrin.h>

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
		m_run_thread{false}, m_decode_mux{},
		m_decode_cv{}, m_decode_th{},
		m_start_decode{false},
		m_use_simd{false},
		m_accurate_idct{true}
	{
		m_out_fifo = std::make_unique<jnk0le::Ringbuffer<u32, RINGBUF_SIZE>>();
	}

	void MDEC::WriteCommand(u32 value) {
		//All writes require the decoding to
		//be halted
		bool has_to_lock = m_start_decode.load();
		if (has_to_lock) {
			m_decode_mux.lock();
		}

		if (m_num_params > 0) {
			m_in_fifo[m_curr_in_pos++] = u16(value);
			m_in_fifo[m_curr_in_pos++] = u16(value >> 16);

			m_num_params -= 1;

			m_stat.data_in_full = m_num_params == 0;

			if (m_stat.data_in_full) {
				switch (m_curr_cmd)
				{
				case MDEC_Cmd::DECODE: {
					if (!m_run_thread) {
						Decode();
					}
					else {
						m_start_decode.store(true);
						m_decode_cv.notify_one();
					}
				}
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

		if (has_to_lock) {
			m_decode_mux.unlock();
		}
	}

	void MDEC::WriteControl(u32 value) {
		bool has_to_lock = m_start_decode.load();
		if (has_to_lock) {
			m_decode_mux.lock();
		}

		if ((value >> 31) & 1) {
			LOG_DEBUG("MDEC", "[MDEC] Reset()");
			Reset();
		}

		if ((value >> 30) & 1) {
			LOG_DEBUG("MDEC", "[MDEC] DmaIn()");
			m_enable_dma0 = true;
		}

		if ((value >> 29) & 1) {
			LOG_DEBUG("MDEC", "[MDEC] DmaOut()");
			m_enable_dma1 = true;
		}

		UpdateDMA0Req();
		UpdateDMA1Req();

		if (has_to_lock) {
			m_decode_mux.unlock();
		}
	}

	u32 MDEC::ReadData() {
		//do not block waiting for
		//decoded data
		if (m_out_fifo->isEmpty()) {
			return 0;
		}
		u32 color = {};
		m_out_fifo->remove(color);

		UpdateDMA0Req();
		UpdateDMA1Req();
		return color;
	}

	u32 MDEC::ReadStat() {
		m_stat.data_out_empty = m_out_fifo->isEmpty();
		return m_stat.raw;
	}

	MDEC::~MDEC() {
		StopDecodeThread();
	}

	void MDEC::StartDecodeThread() {
		if (m_run_thread) {
			return;
		}

		m_start_decode.store(false);
		m_decode_th = std::jthread([this](std::stop_token stop) {
			this->DecodeThread(stop);
		});
		m_run_thread = true;
	}

	void MDEC::StopDecodeThread() {
		if (!m_run_thread) {
			return;
		}

		{
			std::unique_lock<std::mutex> _lk{ m_decode_mux };
			m_decode_th.request_stop();
			m_decode_cv.notify_one();
		}
		
		m_decode_th.join();

		m_run_thread = false;
	}

	void MDEC::Reset() {
		m_stat.raw = STAT_RESET_VALUE;
		m_num_params = 0;
		m_in_fifo.clear();
		m_out_fifo->consumerClear();
		m_enable_dma0 = false;
		m_enable_dma1 = false;
		m_curr_in_pos = 0;
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
			m_out_fifo->consumerClear();

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
		std::copy_n(std::bit_cast<u8*>(m_in_fifo.data()), 64, m_luminance_table.data());
		//for (std::size_t index = 0; index < 64; index += 2) {
		//	auto curr_entry = m_in_fifo[index >> 1];
		//	m_luminance_table[index + 0] = u8(curr_entry >> 0);
		//	m_luminance_table[index + 1] = u8(curr_entry >> 8);
		//}

		if (m_curr_in_pos > 32) {
			FillColor(32);
		}
	}

	void MDEC::FillColor(std::size_t index_base) {
		std::copy_n(std::bit_cast<u8*>(m_in_fifo.data() + index_base), 64, m_color_table.data());

		//for (std::size_t index = 0; index < 64; index += 2) {
		//	auto curr_entry = m_in_fifo[index_base + (index >> 1)];
		//	m_color_table[index + 0] = u8(curr_entry >> 0);
		//	m_color_table[index + 1] = u8(curr_entry >> 8);
		//}
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

	void mono_dma1_update_callback(system_status*, void* mdec) {
		std::bit_cast<MDEC*>(mdec)->UpdateDma1Mono();
	}

	void rgb_dma1_update_callback(system_status*, void* mdec) {
		std::bit_cast<MDEC*>(mdec)->UpdateDma1RGB();
	}

	void MDEC::UpdateDma1Mono() {
		UpdateDMA1Req();
		m_stat.curr_block = CurrBlockType(4);
	}

	void MDEC::UpdateDma1RGB() {
		UpdateDMA1Req();
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

				if (m_start_decode.load()) {
					std::unique_lock<std::mutex> _lk{ m_sys_status->sync_producer_mux };
					m_sys_status->sync_callback_buffer.insert({ 
						mono_dma1_update_callback, std::bit_cast<void*>(this)
					});
				}
				else {
					UpdateDMA1Req();
					m_stat.curr_block = CurrBlockType(4);
				}
				
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
				
				if (m_start_decode.load()) {
					std::unique_lock<std::mutex> _lk{ m_sys_status->sync_producer_mux };
					m_sys_status->sync_callback_buffer.insert({
						rgb_dma1_update_callback, std::bit_cast<void*>(this)
					});
				}
				else {
					UpdateDMA1Req();
				}
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
			!m_out_fifo->isEmpty();

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

		IdctCore(block);
		return block;
	}
	
	std::array<u32, 256> MDEC::YToMono(DecodedBlock& block) const {
		std::array<u32, 256> rgb_block{};

		if (m_use_simd) {
			for (std::size_t idx = 0; idx < 64; idx += 8) {
				__m128i values =  std::bit_cast<__m128i>
					(_mm_loadu_ps(std::bit_cast<float*>(&block[idx])));

				//First make unsigned if requested
				//
				//then perform clamping:
				//temp = max(value, min_val)
				//result = min(temp, max_val)

				if (!m_stat.data_out_signed) {
					values = _mm_add_epi16(values, _mm_set1_epi16(128));

					__m128i temp = _mm_max_epi16(values, _mm_set1_epi16(0));
					values = _mm_min_epi16(temp, _mm_set1_epi16(255));
				}
				else {
					__m128i temp = _mm_max_epi16(values, _mm_set1_epi16(-128));
					values = _mm_min_epi16(temp, _mm_set1_epi16(127));
				}

				//Sign extend 16 bit values to 32 bits
				__m256i packed_words = _mm256_cvtepi16_epi32(values);

				//Store result
				_mm256_store_ps(std::bit_cast<float*>(&rgb_block[idx]), 
					std::bit_cast<__m256>(packed_words));
			}
		}
		else {
			for (std::size_t idx = 0; idx < 64; idx++) {
				i16 curr_val = block[idx];

				if (!m_stat.data_out_signed) {
					curr_val += 0x80;
					curr_val = std::clamp(curr_val, i16(0), i16(255));
				}
				else {
					curr_val = std::clamp(curr_val, i16(-128), i16(127));
				}
				
				rgb_block[idx] = u32(curr_val);
			}
		}

		return rgb_block;
	}

	void MDEC::YUVToRGB(DecodedBlock& block, DecodedBlock& cr, DecodedBlock& cb, std::array<u32, 256>& out, std::size_t xx, std::size_t yy) const {
		if (m_use_simd) {
			for (std::size_t y = 0; y < 8; y++) {
				//Only 4 values are used for each line, load
				//all 4 using 64 bit pointer cast
				auto cr_samples_64 = *std::bit_cast<u64*>(cr.data() + ((xx / 2) + ((y + yy) / 2) * 8));
				auto cb_samples_64 = *std::bit_cast<u64*>(cb.data() + ((xx / 2) + ((y + yy) / 2) * 8));

				//CR samples 0 0 1 1 2 2 3 3
				__m256 cr_samples = {};

				{
					//Duplicate each second value (e.g. from 1 2 ... 7 to 0 0 1 1 2 2 3 3),
					//while doing sign extension and int32 to f32 conversion

					//1) extract and duplicate first two samples, obtaining 0 0 1 1 x x x x
					__m128i cr_samples_low = _mm_shufflelo_epi16(_mm_set_epi64x(0, cr_samples_64), 0b0101'0000);
					//2) extract and duplicate last two samples, obtaining x x x x 2 2 3 3
					__m128i cr_samples_hig = _mm_shufflehi_epi16(_mm_setr_epi64x(0, cr_samples_64), 0b1111'1010);
					//3) Blend the previous vectors to 0 0 1 1 2 2 3 3
					__m128i cr_samples_temp = _mm_blend_epi16(cr_samples_low, cr_samples_hig, 0b11110000);
					//4) Sign extend from i16 to i32
					__m256i cr_samples_i32 = _mm256_cvtepi16_epi32(cr_samples_temp);
					//5) Convert from i32 to f32
					cr_samples = _mm256_cvtepi32_ps(cr_samples_i32);
				}

				//Repeat for CB
				__m256 cb_samples = {};
				
				{
					__m128i cb_samples_low = _mm_shufflelo_epi16(_mm_set_epi64x(0, cb_samples_64), 0b0101'0000);
					__m128i cb_samples_hig = _mm_shufflehi_epi16(_mm_setr_epi64x(0, cb_samples_64), 0b1111'1010);
					__m128i cb_samples_temp = _mm_blend_epi16(cb_samples_low, cb_samples_hig, 0b11110000);
					__m256i cb_samples_i32 = _mm256_cvtepi16_epi32(cb_samples_temp);
					cb_samples = _mm256_cvtepi32_ps(cb_samples_i32);
				}
				
				//Repeat for luma
				__m256 Y = {};

				{
					__m128i Y_i16 = std::bit_cast<__m128i>
						(_mm_loadu_ps(std::bit_cast<float*>(&block[y * 8])));
					__m256i Y_i32 = _mm256_cvtepi16_epi32(Y_i16);
					Y = _mm256_cvtepi32_ps(Y_i32);
				}

				//Combine luma and chroma
				
				__m256 R = _mm256_add_ps(Y, _mm256_mul_ps(_mm256_set1_ps(1.402f), cr_samples));
				__m256 B = _mm256_add_ps(Y, _mm256_mul_ps(_mm256_set1_ps(1.772f), cb_samples));
				__m256 G = _mm256_add_ps(_mm256_mul_ps(_mm256_set1_ps(-0.3437f), cb_samples),
					_mm256_mul_ps(_mm256_set1_ps(-0.7143f), cr_samples));
				G = _mm256_add_ps(Y, G);

				auto mm_clamp = [](__m256 val, float min, float max) {
					__m256 temp = _mm256_max_ps(val, _mm256_set1_ps(min));
					return _mm256_min_ps(temp, _mm256_set1_ps(max));
				};

				if (!m_stat.data_out_signed) {
					//Make unsigned
					R = _mm256_add_ps(R, _mm256_set1_ps(128.0f));
					G = _mm256_add_ps(G, _mm256_set1_ps(128.0f));
					B = _mm256_add_ps(B, _mm256_set1_ps(128.0f));

					R = mm_clamp(R, 0.0f, 255.0f);
					G = mm_clamp(G, 0.0f, 255.0f);
					B = mm_clamp(B, 0.0f, 255.0f);
				}
				else {
					R = mm_clamp(R, -128.0f, 127.0f);
					G = mm_clamp(G, -128.0f, 127.0f);
					B = mm_clamp(B, -128.0f, 127.0f);
				}

				//Convert to i32
				auto R_u32 = _mm256_cvtps_epi32(R);
				auto G_u32 = _mm256_cvtps_epi32(G);
				auto B_u32 = _mm256_cvtps_epi32(B);

				__m256i color = {};

				//Combine RGB data in 8 u32
				color = R_u32;
				color = _mm256_or_epi32(color, _mm256_slli_epi32(G_u32, 8));
				color = _mm256_or_epi32(color, _mm256_slli_epi32(B_u32, 16));

				_mm256_store_ps(std::bit_cast<float*>(out.data() + (xx + (y + yy) * 16)),
					std::bit_cast<__m256>(color));
			}
		}
		else {
			for (std::size_t y = 0; y < 8; y++) {
				for (std::size_t x = 0; x < 8; x++) {
					i32 cr_sample = cr[((x + xx) / 2) + ((y + yy) / 2) * 8];
					i32 cb_sample = cb[((x + xx) / 2) + ((y + yy) / 2) * 8];
					i32 Y = block[x + y * 8];

					i32 R = Y + i32(1.402f * float(cr_sample));
					i32 B = Y + i32(1.772f * float(cb_sample));
					i32 G = Y + i32(-(0.3437f * float(cb_sample)) - (0.7143f * float(cr_sample)));

					if (!m_stat.data_out_signed) {
						R += 0x80;
						G += 0x80;
						B += 0x80;

						R = u8(std::clamp(R, 0, 255));
						G = u8(std::clamp(G, 0, 255));
						B = u8(std::clamp(B, 0, 255));
					}
					else {
						R = u8(std::clamp(R, -128, 127));
						G = u8(std::clamp(G, -128, 127));
						B = u8(std::clamp(B, -128, 127));
					}

					u32 color{};
					color |= (u32(R) << 0);
					color |= (u32(G) << 8);
					color |= (u32(B) << 16);
					out[(x + xx) + (y + yy) * 16] = color;
				}
			}
		}
		
	}
	
#pragma optimize("", off)
	void MDEC::VecToOutFifo(std::array<u32, 256> const& src) {
		size_t required_space = 0;
		switch (m_stat.out_depth)
		{
		case OutputDepth::BIT4:
			required_space = 8;
			break;
		case OutputDepth::BIT8:
			required_space = 16;
			break;
		case OutputDepth::BIT15:
			required_space = 128;
			break;
		case OutputDepth::BIT24:
			required_space = 256;
			break;
		default:
			error::Unreachable();
			break;
		}

		bool is_enough_space = m_out_fifo->writeAvailable() >= required_space;
		if (!m_run_thread && !is_enough_space) {
			LOG_ERROR("MDEC", "[MDEC] NOT ENOUGH SPACE IN OUTPUT BUFFER");
			error::DebugBreak();
		}
		else if(!is_enough_space) {
			LOG_WARN("MDEC", "[MDEC] Waiting for buffer to empty a bit");
			while (m_out_fifo->writeAvailable() < required_space) {
				std::this_thread::yield();
			}
		}

		switch (m_stat.out_depth)
		{
		case OutputDepth::BIT4: {
			if (m_use_simd) {
				for (std::size_t idx = 0; idx < 64; idx += 8) {
					__m256i unpacked = std::bit_cast<__m256i>(_mm256_load_ps(std::bit_cast<float*>(
						src.data() + idx)
					));
					unpacked = _mm256_srli_epi32(unpacked, 4);
					unpacked = _mm256_sllv_epi32(unpacked, _mm256_set_epi32(28, 24, 20, 16, 12, 8, 4, 0));

					//0 4 8 12
					__m128i left = _mm256_extracti128_si256(unpacked, 0);
					//16 20 24 28
					__m128i right = _mm256_extracti128_si256(unpacked, 1);
					// 0|16 4|20 8|24 12|28
					__m128i ored = _mm_or_epi32(left, right);
					// 8|24 12|28 x x
					right = _mm_shuffle_epi32(ored, _MM_SHUFFLE(0, 0, 3, 2));
					// 0|8|16|24 4|12|20|28
					ored = _mm_or_epi32(ored, right);
					// 4|12|20|28 x x ...
					right = _mm_srli_epi64(ored, 32);
					// 0|4|8|12|16|20|24|28
					ored = _mm_or_epi32(ored, right);
					m_out_fifo->insert(uint32_t(_mm_extract_epi32(ored, 0)));
				}
			}
			else {
				for (std::size_t idx = 0; idx < 64; idx += 8) {
					u32 curr_value = src[idx] >> 4;
					curr_value |= (src[idx + 1] >> 4) << 4;
					curr_value |= (src[idx + 2] >> 4) << 8;
					curr_value |= (src[idx + 3] >> 4) << 12;
					curr_value |= (src[idx + 4] >> 4) << 16;
					curr_value |= (src[idx + 5] >> 4) << 20;
					curr_value |= (src[idx + 6] >> 4) << 24;
					curr_value |= (src[idx + 7] >> 4) << 28;
					m_out_fifo->insert(curr_value);
				}
			}
		}
			break;
		case OutputDepth::BIT8: {
			if (m_use_simd) {
				for (size_t idx = 0; idx < 64; idx += 8) {
					__m256i unpacked = std::bit_cast<__m256i>(_mm256_load_ps(std::bit_cast<float*>(src.data() + idx)));
					//0 8 16 24 0 8 16 24
					__m256i left = _mm256_sllv_epi32(unpacked, _mm256_set_epi32(24, 16, 8, 0, 24, 16, 8, 0));
					//16 24 x x 16 24 x x
					__m256i right = _mm256_shuffle_epi32(left, 0b00'00'11'10);

					//0|16 8|24 x x 0|16 8|24 x x
					left = _mm256_or_epi32(left, right);
					//8|24 x x x 8|24 x x x
					right = _mm256_srli_epi64(left, 32);
					//0|8|16|24 x x x 0|8|16|24 x x x 
					left = _mm256_or_epi32(left, right);

					uint32_t dest[2] = {};
					dest[0] = uint32_t(_mm256_extract_epi32(left, 0));
					dest[1] = uint32_t(_mm256_extract_epi32(left, 4));
					m_out_fifo->writeBuff(dest, 2);
				}
			}
			else {
				for (std::size_t idx = 0; idx < 64; idx += 4) {
					u32 curr_value = src[idx];
					curr_value |= (src[idx + 1] << 8);
					curr_value |= (src[idx + 2] << 16);
					curr_value |= (src[idx + 3] << 24);
					m_out_fifo->insert(curr_value);
				}
			}
		}
			break;
		case OutputDepth::BIT15: {
			if (m_use_simd) {
				for (std::size_t idx = 0; idx < 256; idx += 8) {
					//load data for 4 values
					__m256i unpacked = std::bit_cast<__m256i>(
						_mm256_load_ps(std::bit_cast<float*>(src.data() + idx))
					);

					//Extract all R components, drop fraction, mask to max value
					__m256i r = _mm256_and_epi32(_mm256_srli_epi32(unpacked, 3), _mm256_set1_epi32(0x1F));
					//Repeat for GB
					__m256i g = _mm256_and_epi32(_mm256_srli_epi32(unpacked, 11), _mm256_set1_epi32(0x1F));
					__m256i b = _mm256_and_epi32(_mm256_srli_epi32(unpacked, 19), _mm256_set1_epi32(0x1F));

					//Shift GB components
					g = _mm256_slli_epi32(g, 5);
					b = _mm256_slli_epi32(b, 10);

					//Pack RGB channels in 32 bit values
					__m256i colors = _mm256_or_epi32(_mm256_or_epi32(r, g), b);

					//Now we have each color in separate dwords
					//but we need them to be in pairs of two
					if (m_stat.data_out_bit15) {
						colors = _mm256_or_epi32(colors, _mm256_set1_epi32(0x8000));
					}

					//Go from 1x 2x | 3x 4x | 5x 6x | 7x 8x
					//to x2 xx | x4 xx | x6 xx | x8 xx
					__m256i upper_colors = _mm256_srli_epi64(colors, 16);

					//Or the two together to get 12 xx | 34 xx | 56 xx | 78 xx
					//We cannot dump all values in one go, we have blank spaces
					colors = _mm256_or_epi32(colors, upper_colors);

					//Now we have 12 34 | xx xx | 56 78 | xx xx
					colors = _mm256_shuffle_epi32(colors, 0b00'00'10'00);

					//Extract 12 34 and 56 78
					i64 buf[2] = { _mm256_extract_epi64(colors, 0), _mm256_extract_epi64(colors, 2) };
					m_out_fifo->writeBuff(std::bit_cast<u32*>(&buf), 4);
				}
			}
			else {
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
					m_out_fifo->insert(curr_value);
				}
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
					m_out_fifo->insert(color);
					//color = GB--
					color = (temp >> 8);
					state = 2;
				}
					break;
				case 2: {
					u32 temp = src[idx];
					//color = GBRG
					color |= ((temp & 0xFFFF) << 16);
					m_out_fifo->insert(color);
					//color = B---
					color = (temp >> 16);
					state = 3;
				}
					break;
				case 3: {
					u32 temp = src[idx];
					//color = BRGB
					color |= ((temp & 0xFFFFFF) << 8);
					m_out_fifo->insert(color);

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
#pragma optimize("", on)

	void MDEC::IdctCore(std::array<i16, 64>& blk) {
		std::array<i64, 64> temp{};

		if (m_use_simd) {
			for (std::size_t y = 0; y < 8; y++) {
				for (std::size_t x = 0; x < 8; x += 4) {
					__m256i sum = {};
					for (std::size_t z = 0; z < 8; z++) {
						__m128i left = _mm_cvtepi16_epi32(_mm_set1_epi64x(
							*std::bit_cast<u64*>(blk.data() + (z * 8 + x))
						));
						__m128i right = _mm_set1_epi32(i32(m_scale_table[y * 8 + z]));
						__m256i mul_res = _mm256_cvtepi32_epi64(_mm_mullo_epi32(left, right));

						sum = _mm256_add_epi64(sum, mul_res);
					}
					_mm256_storeu_ps(
						std::bit_cast<float*>(temp.data() + (y*8+x)),
						std::bit_cast<__m256>(sum)
					);
				}
			}

			if (!m_accurate_idct) {
				for (std::size_t x = 0; x < 8; x++) {
					for (std::size_t y = 0; y < 8; y++) {
						__m256i sum = {};
						for (std::size_t z = 0; z < 8; z += 4) {
							//Load 4 consecutive 64 bit values
							__m256i left_temp = std::bit_cast<__m256i>(_mm256_load_ps(
								std::bit_cast<float*>(temp.data() + (y*8+z))
							));
							//Drop lower 8 bits, to make sure that the
							//result can fit in 32 bits
							left_temp = _mm256_srli_epi64(left_temp, 8);
							//Convert to 32 bits and fill new vector
							__m128i left = _mm_set_epi32(
								i32(_mm256_extract_epi64(left_temp, 3)),
								i32(_mm256_extract_epi64(left_temp, 2)),
								i32(_mm256_extract_epi64(left_temp, 1)),
								i32(_mm256_extract_epi64(left_temp, 0)));
							//Load 4 16 bit values and 
							__m128i right = _mm_cvtepi16_epi32(_mm_set1_epi64x(
								*std::bit_cast<u64*>(m_scale_table.data() + (x * 8 + z))
							));
							__m256i mul_res = _mm256_cvtepi32_epi64(_mm_mullo_epi32(left, right));
							
							sum = _mm256_add_epi64(sum, _mm256_slli_epi64(mul_res, 8));
						}

						i64 true_sum = _mm256_extract_epi64(sum, 0) +
							_mm256_extract_epi64(sum, 1) +
							_mm256_extract_epi64(sum, 2) +
							_mm256_extract_epi64(sum, 3);

						int round = (true_sum >> 31) & 1;
						blk[x + y * 8] = u16((true_sum >> 32) + round);
					}
				}
			}
		}
		else {
			for (std::size_t y = 0; y < 8; y++) {
				for (std::size_t x = 0; x < 8; x++) {
					i64 sum = 0;
					for (std::size_t z = 0; z < 8; z++) {
						sum += i64(blk[x + z * 8] * m_scale_table[y * 8 + z]);
					}
					temp[x + y * 8] = sum;
				}
			}
		}

		if (!m_use_simd || m_accurate_idct) {
			//Repeat with src/dst exchanged
			for (std::size_t x = 0; x < 8; x++) {
				for (std::size_t y = 0; y < 8; y++) {
					i64 sum = 0;
					for (std::size_t z = 0; z < 8; z++) {
						sum += i64(temp[y * 8 + z] * m_scale_table[x * 8 + z]);
					}

					int round = (sum >> 31) & 1;
					blk[x + y * 8] = u16((sum >> 32) + round);
				}
			}
		}
	}

	void MDEC::DecodeThread(std::stop_token stop) {
		while (!stop.stop_requested()) {
			std::unique_lock<std::mutex> _lk{ m_decode_mux };
			m_decode_cv.wait(_lk, [this, stop]() { return this->m_start_decode.load() ||
				stop.stop_requested(); });

			if (stop.stop_requested()) {
				break;
			}

			Decode();
			m_start_decode.store(false);
		}
	}

}