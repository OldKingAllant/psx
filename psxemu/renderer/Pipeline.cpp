#include "Pipeline.hpp"

#include <GL/glew.h>

#include <common/Errors.hpp>

namespace psx::video {
	void PerformDraw(Primitive primitive_type, u32 offset, u32 count) {
		uint32_t ogl_primitive = 0;

		switch (primitive_type)
		{
		case psx::video::Primitive::TRIANGLES:
			ogl_primitive = GL_TRIANGLES;
			break;
		case psx::video::Primitive::LINES:
			ogl_primitive = GL_LINES;
			break;
		default:
			error::DebugBreak();
			break;
		}

		glDrawArrays(ogl_primitive, offset, count);
	}
}