#include "AudioBackend.hpp"

#include <psxemu/include/psxemu/Logger.hpp>
#include <psxemu/include/psxemu/LoggerMacros.hpp>

#include <bit>

namespace psx {
	void audio_callback(void* userdata, uint8_t* stream, int len) {
		AudioBackend* device = std::bit_cast<AudioBackend*>(userdata);
		i16* dest = std::bit_cast<i16*>(stream);

		std::unique_lock _lk{ device->m_mux };

		len /= 2;
		std::fill_n(dest, len, 0x00);

		auto to_read = std::min<size_t>(device->m_audio_buffer.size(), len);
		
		for (size_t i = 0; i < to_read; i++) {
			auto sample = device->m_audio_buffer.front();
			device->m_audio_buffer.pop_front();
			dest[i] = sample;
		}
	}

	AudioBackend::AudioBackend() {
		SDL_AudioSpec wanted{};
		wanted.freq = 44100;
		wanted.format = AUDIO_S16;
		wanted.callback = audio_callback;
		wanted.channels = 2;
		wanted.userdata = std::bit_cast<void*>(this);
		wanted.samples = 512;

		m_dev_id = SDL_OpenAudioDevice(nullptr, 0, &wanted, &m_dev_specs, 0);

		LOG_INFO("AUDIO", "[AUDIO] (SDL) Silence value is {:#x}", m_dev_specs.silence);

		if (m_dev_specs.format != wanted.format || m_dev_specs.freq != wanted.freq) {
			LOG_WARN("AUDIO", "[AUDIO] (SDL) Obtained output device with specs != from wanted");
		}

		if (m_dev_id == 0) {
			LOG_ERROR("AUDIO", "[AUDIO] (SDL) Could not open device: {}", SDL_GetError());
			std::exit(1);
		}
	}

	void AudioBackend::PushSamples(i16* buf, size_t len) {
		constexpr size_t MAX_AUDIO_BUFFER_SIZE = 8192;
		std::unique_lock lk{ m_mux };
		if (m_audio_buffer.size() != 0) {
			LOG_WARN("AUDIO", "[AUDIO] Non-empty audio buffer");
		}
		if (m_audio_buffer.size() >= MAX_AUDIO_BUFFER_SIZE) {
			m_audio_buffer.clear();
		}
		std::copy_n(buf, len, std::back_inserter(m_audio_buffer));
		m_cv.notify_one();
	}

	AudioBackend::~AudioBackend() {
		if (m_dev_id != 0) {
			Stop();
			m_dev_id = 0;
		}
	}

	void AudioBackend::Start() {
		if (m_dev_id == 0) {
			return;
		}
		SDL_PauseAudioDevice(m_dev_id, 0);
	}

	void AudioBackend::Pause() {
		if (m_dev_id == 0) {
			return;
		}
		SDL_PauseAudioDevice(m_dev_id, 1);
	}

	void AudioBackend::Stop() {
		if (m_dev_id == 0) {
			return;
		}
		Pause();
		SDL_CloseAudioDevice(m_dev_id);
	}
}