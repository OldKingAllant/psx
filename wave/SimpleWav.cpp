#include "SimpleWav.hpp"

#include <bit>

namespace psx::wav {
	SimpleWav::SimpleWav(SimpleWav&& _prev) noexcept :
		m_fd_handle{std::move(_prev.m_fd_handle)},
		m_data{std::move(_prev.m_data)},
		m_format{_prev.m_format},
		m_last_written_pos{_prev.m_last_written_pos}
	{}

	SimpleWav& SimpleWav::operator=(SimpleWav && _other) noexcept {
		m_fd_handle = std::move(_other.m_fd_handle);
		m_data = std::move(_other.m_data);
		m_format = _other.m_format;
		m_last_written_pos = _other.m_last_written_pos;

		return *this;
	}

	std::expected<SimpleWav, WavError> SimpleWav::CreateWav(
		std::filesystem::path const& _path, 
		SampleFormat _format, 
		uint16_t _channels, 
		uint16_t _bits_sample, 
		uint16_t _sample_rate) {
		if (SampleFormat::PCM != _format &&
			SampleFormat::FLOAT != _format) {
			return std::unexpected{ WavError::INVALID_SAMPLE_FORMAT };
		}

		if(0 == _channels) {
			return std::unexpected{ WavError::INVALID_CHANNELS };
		}

		if (0 != (_bits_sample & 0x7)) {
			return std::unexpected{ WavError::INVALID_BITS_SAMPLE };
		}

		if (_format == SampleFormat::FLOAT && 32 != _bits_sample) {
			return std::unexpected{ WavError::INVALID_BITS_SAMPLE };
		}

		if (0 == _sample_rate) {
			return std::unexpected{ WavError::INVALID_SAMPLE_RATE };
		}

		SimpleWav wav{};
		wav.m_fd_handle = std::ofstream{ _path, std::ios::binary };

		if (!wav.m_fd_handle.is_open()) {
			return std::unexpected{ WavError::CREATE_FAILED };
		}

		DataFormat data_format{};
		data_format.format = _format;
		data_format.bits_sample = _bits_sample;
		data_format.channels = _channels;
		data_format.sample_rate = _sample_rate;
		wav.m_format = data_format;

		return wav;
	}

	std::optional<WavError> SimpleWav::WriteSamples(std::span<uint8_t> _data) {
		const uint32_t sample_size_bytes = m_format.bits_sample >> 3;
		const size_t expected_block_size = (size_t)sample_size_bytes * (size_t)m_format.channels;

		if (0 != (_data.size() % expected_block_size)) {
			return WavError::NOT_ENOUGH_DATA;
		}

		try {
			m_data.append_range(_data);
		}
		catch (std::bad_alloc const&) {
			return WavError::ALLOC_FAILED;
		}
		return std::nullopt;
	}

	void SimpleWav::FlushData() {
		if (!m_fd_handle.is_open()) {
			return;
		}
		if (0 == m_data.size()) {
			return;
		}

		WriteData( WriteHeaders() );
		m_last_written_pos += m_data.size();
		m_data.clear();
		m_fd_handle.flush();
	}

	SimpleWav::~SimpleWav() {
		FlushData();
	}

	SimpleWav::SimpleWav() :
		m_fd_handle{},
		m_data{},
		m_format{},
		m_last_written_pos{} {}

	size_t SimpleWav::WriteHeaders() {
		RiffHeader riff{};
		FmtChunk fmt{};
		ChunkHeader data_header{};

		const uint32_t sample_size_bytes = m_format.bits_sample >> 3;
		const uint32_t num_blocks = (uint32_t)(
			(m_last_written_pos + m_data.size()) / m_format.channels / sample_size_bytes
			);

		constexpr char RIFF[] = {'R', 'I', 'F', 'F'};
		constexpr char FMT[] = { 'f', 'm', 't', ' ' };
		std::copy_n(RIFF, sizeof(RIFF), riff.header.id);
		riff.header.chunk_size = 4 + 24 + (8 +
			sample_size_bytes * m_format.channels * num_blocks);
		if (0 != (riff.header.chunk_size & 1)) {
			riff.header.chunk_size += 1;
		}
		std::copy_n(FMT, sizeof(FMT), fmt.header.id);
		fmt.header.chunk_size = 16;
		fmt.sample_format = m_format.format;
		fmt.n_channels = m_format.channels;
		fmt.sample_rate = m_format.sample_rate;
		fmt.byte_rate = fmt.sample_rate * sample_size_bytes * fmt.n_channels;
		fmt.data_block_size = sample_size_bytes * fmt.n_channels;
		fmt.bits_sample = m_format.bits_sample;

		constexpr char DATA[] = { 'd', 'a', 't', 'a' };
		std::copy_n(DATA, sizeof(DATA), data_header.id);
		data_header.chunk_size = sample_size_bytes * m_format.channels * num_blocks;

		m_fd_handle.seekp(0, std::ios::beg);
		m_fd_handle.write(std::bit_cast<const char*>(&riff), sizeof(RiffHeader));
		m_fd_handle.write(std::bit_cast<const char*>(&fmt), sizeof(FmtChunk));
		m_fd_handle.write(std::bit_cast<const char*>(&data_header), sizeof(ChunkHeader));
		return m_fd_handle.tellp();
	}

	size_t SimpleWav::WriteData(size_t _offset) {
		const uint32_t sample_size_bytes = m_format.bits_sample >> 3;
		const uint32_t num_blocks = (uint32_t)(m_data.size() /
			m_format.channels);

		const size_t abs_file_offset = _offset + m_last_written_pos;
		m_fd_handle.seekp(abs_file_offset, std::ios::beg);
		m_fd_handle.write(std::bit_cast<const char*>(m_data.data()), m_data.size());
		if (0 != ((m_last_written_pos + m_data.size()) & 1)) {
			m_fd_handle.write("\x00", 1);
		}

		return m_fd_handle.tellp();
	}
}