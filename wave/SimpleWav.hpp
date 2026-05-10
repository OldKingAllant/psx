#pragma once

#include <fstream>
#include <vector>
#include <filesystem>
#include <expected>
#include <span>
#include <optional>

namespace psx::wav {
	struct ChunkHeader {
		char id[4];
		uint32_t chunk_size;
	};

	struct RiffHeader {
		ChunkHeader header;
		char wave_id[4] = { 'W', 'A', 'V', 'E' };
	};

	enum class SampleFormat : uint16_t {
		PCM   = 0x1,
		FLOAT = 0x3
	};

#pragma pack(push, 1)
	struct FmtChunk {
		ChunkHeader header;
		SampleFormat sample_format;
		uint16_t n_channels;
		uint32_t sample_rate;
		uint32_t byte_rate;
		uint16_t data_block_size;
		uint16_t bits_sample;
	};
#pragma pack(pop)

	enum class WavError {
		INVALID_SAMPLE_FORMAT,
		INVALID_CHANNELS,
		INVALID_BITS_SAMPLE,
		INVALID_SAMPLE_RATE,
		CREATE_FAILED,
		ALLOC_FAILED,
		NOT_ENOUGH_DATA
	};

	class SimpleWav {
	public :
		SimpleWav(SimpleWav&& _prev) noexcept;
		SimpleWav& operator=(SimpleWav&& _other) noexcept;

		static std::expected<SimpleWav, WavError> CreateWav(
			std::filesystem::path const& _path,
			SampleFormat _format,
			uint16_t _channels,
			uint16_t _bits_sample,
			uint16_t _sample_rate);

		std::optional<WavError> WriteSamples(std::span<uint8_t> _data);

		void FlushData();

		~SimpleWav();

		struct DataFormat {
			SampleFormat format;
			uint16_t     channels;
			uint16_t     bits_sample;
			uint16_t     sample_rate;
		};

		inline SampleFormat GetSampleFormat() const {
			return m_format.format;
		}

		inline uint16_t GetNumChannels() const {
			return m_format.channels;
		}

		inline uint16_t GetBitsPerSample() const {
			return m_format.bits_sample;
		}

		inline uint16_t GetSampleRate() const {
			return m_format.sample_rate;
		}

	private :
		SimpleWav();

		SimpleWav(SimpleWav const&) = delete;
		SimpleWav& operator=(SimpleWav const&) = delete;

		size_t WriteHeaders();
		size_t WriteData(size_t _offset);

	private :
		std::ofstream        m_fd_handle;
		std::vector<uint8_t> m_data;
		DataFormat			 m_format;
		size_t               m_last_written_pos;
	};
}