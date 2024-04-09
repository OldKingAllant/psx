#include "SdlContext.hpp"

#include <stdexcept>

#include <SDL2/SDL.h>

namespace psx::video {
	void SdlInit() {
		if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK)) {
			std::string the_error = SDL_GetError();
			throw std::runtime_error("SDL_Init() failed : " + the_error);
		}
	}

	void SdlShutdown() {
		SDL_Quit();
	}
}