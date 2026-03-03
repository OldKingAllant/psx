#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_audio.h>

#include <thirdparty/ringbuffer/ringbuffer.hpp>
#include <mutex>
#include <condition_variable>

#include <common/Defs.hpp>

namespace psx {
	class AudioBackend {
	public :
		AudioBackend();

		void PushSamples(i16* buf, size_t len);

		friend void audio_callback(void* userdata, uint8_t* stream, int len);

		~AudioBackend();

		void Start();
		void Pause();
		void Stop();

	private :
		SDL_AudioDeviceID m_dev_id;
		SDL_AudioSpec m_dev_specs;
		jnk0le::Ringbuffer<i16, 4096> m_audio_buffer;
		std::condition_variable m_cv;
		std::mutex m_mux;
	};
}