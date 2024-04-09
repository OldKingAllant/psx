#include "GlLoad.hpp"

#include <fmt/format.h>

namespace psx::video {
	bool gl_is_init = false;
	bool gl_debug_en = false;

	bool GlIsInit() {
		return gl_is_init;
	}

	bool GlInit() {
		if (GlIsInit())
			return true;

		GLenum error = glewInit();

		if (error != GLEW_OK) {
			fmt::println("[RENDERER] glewInit() failed : {}",
				(const char*)glewGetErrorString(error));
			return false;
		}

		fmt::println("[RENDERER] Using GLEW version {}",
			(const char*)glewGetString(GLEW_VERSION));

		gl_is_init = true;

		return true;
	}

	std::vector<GLenum> GlGetErrors() {
		std::vector<GLenum> errors = {};
		GLenum err{};

		while ((err = glGetError()) != GL_NO_ERROR)
		{
			errors.push_back(err);
		}

		return errors;
	}

	void GlEnableDebugOutput() {
		gl_debug_en = true;
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
		glEnable(GL_DEBUG_OUTPUT);
	}

	void GlSetDebugMessageCallback(DebugMessageCallback callback, void* userdata) {
		glDebugMessageCallback(callback, userdata);
	}

	bool GlIsDebugEnabled() {
		return gl_debug_en;
	}
}