#pragma once

#include <vector>
#include <string>
#include <GL/glew.h>

namespace psx::video {
	/// <summary>
	/// Get current GLEW status
	/// </summary>
	/// <returns>true if GLEW has already been init</returns>
	bool GlIsInit();

	/// <summary>
	/// Load GLEW
	/// </summary>
	bool GlInit();

	/// <summary>
	/// Retrieve list of all OpenGL errors
	/// </summary>
	/// <returns>Vector of GLenum</returns>
	std::vector<GLenum> GlGetErrors();

	/// <summary>
	/// Enable Vendor-specific logging
	/// </summary>
	void GlEnableDebugOutput();

	using DebugMessageCallback = void(GLAPIENTRY*)(GLenum source,
		GLenum type,
		GLuint id,
		GLenum severity,
		GLsizei length,
		const GLchar* message,
		const void* userParam);

	void GlSetDebugMessageCallback(DebugMessageCallback callback, void* userdata);

	bool GlIsDebugEnabled();
}