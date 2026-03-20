#pragma once

#include "FirstOrderLPF.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_audio.h>

#include <mutex>
#include <condition_variable>

#include <thirdparty/ringbuffer/ringbuffer.hpp>

#include <common/Defs.hpp>

namespace psx {
	class AudioBackend {
	public :
		AudioBackend(size_t buffer_limit);

		void PushSamples(i16* buf, size_t len);

		friend void audio_callback(void* userdata, uint8_t* stream, int len);

		~AudioBackend();

		void Start();
		void Pause();
		void Stop();

		void SyncToAudio(bool sync_audio) {
			m_sync_to_audio = sync_audio;
		}

		bool IsAudioSynced() const {
			return m_sync_to_audio;
		}

		static constexpr i32 AUDIO_FREQUENCY = 44100;
		static constexpr i32 AUDIO_CHANNELS = 2;

	private :
		SDL_AudioDeviceID m_dev_id;
		SDL_AudioSpec m_dev_specs;
		jnk0le::Ringbuffer<i16, 8192> m_audio_buffer;
		std::mutex m_mux;
		std::condition_variable m_cv;
		size_t m_buffer_limit;
		bool m_sync_to_audio;

		LowPassFilter<i16> m_left_lpf;
		LowPassFilter<i16> m_right_lpf;
	};
}