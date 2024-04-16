#include "VertexBuffer.hpp"

#include <GL/glew.h>

#include <common/Errors.hpp>

namespace psx::video {
	u32 CreateVertexArray() {
		u32 id = 0;
		glGenVertexArrays(1, &id);
		return id;
	}

	u32 VertexAttributeToOpenGL(VertexAttributeType attr) {
		switch (attr)
		{
		case psx::video::VertexAttributeType::BYTE:
			return GL_BYTE;
			break;
		case psx::video::VertexAttributeType::UNSIGNED_BYTE:
			return GL_UNSIGNED_BYTE;
			break;
		case psx::video::VertexAttributeType::SHORT:
			return GL_SHORT;
			break;
		case psx::video::VertexAttributeType::UNSIGNED_SHORT:
			return GL_UNSIGNED_SHORT;
			break;
		case psx::video::VertexAttributeType::INT:
			return GL_INT;
			break;
		case psx::video::VertexAttributeType::UNSIGNED_INT:
			return GL_UNSIGNED_INT;
			break;
		case psx::video::VertexAttributeType::FLOAT:
			return GL_FLOAT;
			break;
		case psx::video::VertexAttributeType::UVEC2:
		case psx::video::VertexAttributeType::UVEC3:
			return GL_UNSIGNED_INT;
			break;
		default:
			error::DebugBreak();
			break;
		}

		return GL_FLOAT;
	}

	void MakeCurrentVertexArray(u32 id) {
		glBindVertexArray(id);
	}

	void MakeCurrentArrayBuffer(u32 id) {
		glBindBuffer(GL_ARRAY_BUFFER, id);
	}

	void SetVertexAttributes(std::vector<VertexAttribute> const& attributes, u32 vert_size) {
		u32 index = 0;
		
		for (auto const& attribute : attributes) {
			auto type = VertexAttributeToOpenGL(attribute.type);
			auto offset = (const void*)attribute.offset;
			auto size = VERTEX_ATTRIB_SIZES[(u32)attribute.type];

			switch (type)
			{
			case GL_FLOAT:
				glVertexAttribPointer(index, size, type, GL_FALSE,
					vert_size, offset);
				break;
			case GL_DOUBLE:
				glVertexAttribLPointer(index, size, type,
					vert_size, offset);
				break;
			default:
				glVertexAttribIPointer(index, size, type,
					vert_size, offset);
				break;
			}
			
			glEnableVertexAttribArray(index);

			index++;
		}
	}

	void DeleteVertexArray(u32& id) {
		glDeleteVertexArrays(1, &id);
	}
}