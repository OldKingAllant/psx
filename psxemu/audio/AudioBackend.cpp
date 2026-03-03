#include "AudioBackend.hpp"

#include <psxemu/include/psxemu/Logger.hpp>
#include <psxemu/include/psxemu/LoggerMacros.hpp>

#include <bit>

namespace psx {
	void audio_callback(void* userdata, uint8_t* stream, int len) {
		AudioBackend* device = std::bit_cast<AudioBackend*>(userdata);
		i16* dest = std::bit_cast<i16*>(stream);

		std::unique_lock _lk{ device->m_mux };

		auto avail = device->m_audio_buffer.readAvailable();
		len /= 2;
		auto to_read = std::min<size_t>(avail, len);
		
		device->m_audio_buffer.readBuff(dest, to_read);

		_lk.unlock();
		device->m_cv.notify_one();
	}

	AudioBackend::AudioBackend() {
		SDL_AudioSpec wanted{};
		wanted.freq = 44100;
		wanted.format = AUDIO_S16;
		wanted.callback = audio_callback;
		wanted.channels = 2;
		wanted.userdata = std::bit_cast<void*>(this);
		wanted.samples = 2048;

		m_dev_id = SDL_OpenAudioDevice(nullptr, 0, &wanted, &m_dev_specs, 0);

		if (m_dev_id == 0) {
			LOG_ERROR("AUDIO", "[AUDIO] (SDL) Could not open device: {}", SDL_GetError());
			std::exit(1);
		}
	}

	void AudioBackend::PushSamples(i16* buf, size_t len) {
		std::unique_lock lk{ m_mux };
		m_cv.wait(lk, [this, len]() {return m_audio_buffer.writeAvailable() >= len; });
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