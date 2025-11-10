#include "SdlContext.hpp"

#include <stdexcept>

#include <SDL2/SDL.h>

namespace psx::video {
	void SdlInit() {
		if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK)) {
			std::string the_error = SDL_GetError();
			throw std::runtime_error("SDL_Init() failed : " + the_error);
		}

		SDL_GL_LoadLibrary(nullptr);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
			SDL_GL_CONTEXT_PROFILE_CORE);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	}

	void SdlShutdown() {
		SDL_Quit();
	}
}