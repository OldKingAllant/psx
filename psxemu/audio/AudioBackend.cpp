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

		auto to_read = std::min<size_t>(device->m_audio_buffer.readAvailable(), len);
		
		for (size_t i = 0; i < to_read; i += 2) {
			i16 l, r{};
			device->m_audio_buffer.remove(&l);
			device->m_audio_buffer.remove(&r);

			l = device->m_left_lpf.Filter(l);
			r = device->m_right_lpf.Filter(r);

			dest[i] = l;
			dest[i + 1] = r;
		}

		device->m_cv.notify_one();
	}

	AudioBackend::AudioBackend(size_t buffer_limit) : m_buffer_limit{ buffer_limit }, m_sync_to_audio{true},
		m_left_lpf{ AudioBackend::AUDIO_FREQUENCY, 42000 },
		m_right_lpf{ AudioBackend::AUDIO_FREQUENCY, 42000 } {
		if (buffer_limit >= m_audio_buffer.writeAvailable()) {
			LOG_ERROR("AUDIO", "[AUDIO] (SDL) REQUESTED BUFFER SIZE IS GREATER THAN RINGBUFFER MAX");
			std::exit(1);
		}

		SDL_AudioSpec wanted{};
		wanted.freq = AUDIO_FREQUENCY;
		wanted.format = AUDIO_S16;
		wanted.callback = audio_callback;
		wanted.channels = AUDIO_CHANNELS;
		wanted.userdata = std::bit_cast<void*>(this);
		wanted.samples = (Uint16)buffer_limit / 2;

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
		std::unique_lock lk{ m_mux };

		if (m_sync_to_audio) {
			if (m_audio_buffer.writeAvailable() < len || m_audio_buffer.readAvailable() >= m_buffer_limit) {
				m_cv.wait(lk, [this, len]() {
					return m_audio_buffer.writeAvailable() >= len && m_audio_buffer.readAvailable() < m_buffer_limit; 
				});
			}
		}
		
		m_audio_buffer.writeBuff(buf, len);
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