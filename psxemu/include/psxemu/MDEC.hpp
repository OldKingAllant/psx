#pragma once

#include <common/Defs.hpp>
#include <common/Queue.hpp>
#include <common/CpuFeatures.hpp>

#include <array>
#include <vector>
#include <optional>
#include <stop_token>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <memory>

#include <thirdparty/ringbuffer/ringbuffer.hpp>

class DebugView;

namespace psx {
	struct system_status;

	static constexpr u32 MDEC0_ADDRESS = 0x820;
	static constexpr u32 MDEC1_ADDRESS = 0x824;

	enum class CurrBlockType : u8 {
		Y1,
		Y2,
		Y3,
		Y4,
		CR,
		CB
	};

	enum class OutputDepth : u8 {
		BIT4,
		BIT8,
		BIT24,
		BIT15
	};

	union MDEC_Status {
#pragma pack(push, 1)
		struct {
			u16 missing_params       : 16;
			CurrBlockType curr_block : 3;
			u8 : 4;
			bool data_out_bit15     : 1;
			bool data_out_signed     : 1;
			OutputDepth out_depth    : 2;
			bool data_out_request    : 1;
			bool data_in_request     : 1;
			bool cmd_busy            : 1;
			bool data_in_full        : 1;
			bool data_out_empty      : 1;
		};
#pragma pack(pop)

		u32 raw;
	};

	enum class MDEC_Cmd {
		IDLE      = 0,
		DECODE    = 1,
		SET_QUANT = 2,
		SET_SCALE = 3
	};

	class MDEC {
	public :
		MDEC(system_status* sys_status);

		void WriteCommand(u32 value);
		void WriteControl(u32 value);

		u32 ReadData();
		u32 ReadStat();

		~MDEC();

		void StartDecodeThread();
		void StopDecodeThread();

		inline void UseSimd() {
			m_use_simd = true;
		}

		inline void UseInaccurateIdct() {
			m_accurate_idct = false;
		}

		static constexpr u32 STAT_RESET_VALUE = 0x80040000;

		static constexpr std::array<i16, 64> DEFAULT_SCALE_TABLE = {
			i16(0x5A82), i16(0x5A82), i16(0x5A82), i16(0x5A82), i16(0x5A82), i16(0x5A82), i16(0x5A82), i16(0x5A82),
            i16(0x7D8A), i16(0x6A6D), i16(0x471C), i16(0x18F8), i16(0xE707), i16(0xB8E3), i16(0x9592), i16(0x8275),
            i16(0x7641), i16(0x30FB), i16(0xCF04), i16(0x89BE), i16(0x89BE), i16(0xCF04), i16(0x30FB), i16(0x7641),
            i16(0x6A6D), i16(0xE707), i16(0x8275), i16(0xB8E3), i16(0x471C), i16(0x7D8A), i16(0x18F8), i16(0x9592),
            i16(0x5A82), i16(0xA57D), i16(0xA57D), i16(0x5A82), i16(0x5A82), i16(0xA57D), i16(0xA57D), i16(0x5A82),
            i16(0x471C), i16(0x8275), i16(0x18F8), i16(0x6A6D), i16(0x9592), i16(0xE707), i16(0x7D8A), i16(0xB8E3),
            i16(0x30FB), i16(0x89BE), i16(0x7641), i16(0xCF04), i16(0xCF04), i16(0x7641), i16(0x89BE), i16(0x30FB),
            i16(0x18F8), i16(0xB8E3), i16(0x6A6D), i16(0x8275), i16(0x7D8A), i16(0x9592), i16(0x471C), i16(0xE707),
		};

		static constexpr std::array<u8, 64> ZIGZAG = {
			0 ,1 ,5 ,6 ,14,15,27,28,
            2 ,4 ,7 ,13,16,26,29,42,
            3 ,8 ,12,17,25,30,41,43,
            9 ,11,18,24,31,40,44,53,
            10,19,23,32,39,45,52,54,
            20,22,33,38,46,51,55,60,
            21,34,37,47,50,56,59,61,
            35,36,48,49,57,58,62,63
		};

		static constexpr auto ZAGZIG = []() {
			std::array<u8, 64> table{};

			for (std::size_t idx = 0; idx < 64; idx++) {
				table[ZIGZAG[idx]] = u8(idx);
			}

			return table;
		}();

		static constexpr std::array<double, 8> SCALEFACTOR = {
			1.000000000, 1.387039845, 1.306562965, 1.175875602,
            1.000000000, 0.785694958, 0.541196100, 0.275899379
		};

		static constexpr std::array<double, 64> SCALEZAG = []() {
			std::array<double, 64> table{};

			for (std::size_t y = 0; y < 8; y++) {
				for (std::size_t x = 0; x < 8; x++) {
					table[ZIGZAG[x + y * 8]] =
						SCALEFACTOR[x] * SCALEFACTOR[y] / 8;
				}
			}

			return table;		
		}();

		using block_iter = std::vector<u16>::const_iterator;
		using DecodedBlock = std::array<i16, 64>;

		friend class DebugView;

	private :
		void Reset();
		void CmdStart(u32 value);

		void FillLuminance();
		void FillColor(std::size_t index_base);
		void FillScale();
		void Decode();

		void UpdateDMA0Req();
		void UpdateDMA1Req();

		std::optional<DecodedBlock> DecodeSingleBlock(block_iter& iter, std::array<u8, 64> const& qt);
		std::array<u32, 256> YToMono(DecodedBlock& block) const;
		void YUVToRGB(DecodedBlock& block, DecodedBlock& cr, DecodedBlock& cb, std::array<u32, 256>& out, std::size_t xx, std::size_t yy) const;

		void VecToOutFifo(std::array<u32, 256> const& src);

		void IdctCore(std::array<i16, 64>& blk);

		void DecodeThread(std::stop_token);

		friend void mono_dma1_update_callback(system_status*, void*);
		friend void rgb_dma1_update_callback(system_status*, void*);

		void UpdateDma1Mono();
		void UpdateDma1RGB();

	private :
		MDEC_Status m_stat;
		bool m_enable_dma0;
		bool m_enable_dma1;
		u32  m_num_params;
		std::array<u8, 64> m_luminance_table;
		std::array<u8, 64> m_color_table;
		std::array<i16, 64> m_scale_table;
		MDEC_Cmd m_curr_cmd;
		system_status* m_sys_status;
		bool m_can_use_fast_idct;

		std::vector<u16> m_in_fifo;

		constexpr static size_t RINGBUF_SIZE = 524288;
		std::unique_ptr<jnk0le::Ringbuffer<u32, RINGBUF_SIZE>> m_out_fifo;
		std::size_t m_curr_in_pos;

		bool m_run_thread;
		std::mutex m_decode_mux;
		std::condition_variable m_decode_cv;
		std::jthread m_decode_th;
		std::atomic_bool m_start_decode;

		bool m_use_simd;
		bool m_accurate_idct;
	};
}