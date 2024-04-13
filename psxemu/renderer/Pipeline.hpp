#pragma once

#include <common/Defs.hpp>

#include "VertexBuffer.hpp"
#include "Shader.hpp"

#include <string>

namespace psx::video {
	enum class Primitive {
		TRIANGLES,
		LINES
	};

	static constexpr u32 vertex_count_per_primitive(Primitive ty) {
		switch (ty)
		{
		case psx::video::Primitive::TRIANGLES:
			return 3;
			break;
		case psx::video::Primitive::LINES:
			return 2;
			break;
		default:
			break;
		}

		throw "Invalid primitve";

		return 0;
	}

	static constexpr u32 MAX_VERTEX_COUNT = 9000;
	static constexpr u32 MAX_PRIMITIVE_COUNT = 3000;

	void PerformDraw(Primitive primitive_type, u32 offset, u32 count);

	template <Primitive Prim, Vertex Vert, typename PrimitiveData, typename... Uniforms>
	class Pipeline {
	public :
		Pipeline(std::string const& shader_loc, std::string const& shader_name) :
			m_buffer{MAX_VERTEX_COUNT}, m_shader(shader_loc, shader_name),
			m_per_primitive_data{}, m_curr_primitive{} {}

		void Draw(u32 offset, u32 vertex_count, std::pair<std::string_view, Uniforms>... uniforms) {
			m_shader.BindProgram();
			(m_shader.UpdateUniform(uniforms.first, uniforms.second), ...);
			m_buffer.Bind();

			PerformDraw(Prim, offset, vertex_count);
			
			m_buffer.Unbind();
		}

		void PushVertex(Vert vertex) {
			m_buffer.PushVertex(vertex);
		}

		void ClearVertices() {
			m_buffer.Clear();
		}

		u32 VertexCount() const {
			return m_buffer.VertexCount();
		}

		void AddPrimitiveData(PrimitiveData data) {
			if (m_curr_primitive >= MAX_PRIMITIVE_COUNT)
				return;

			m_per_primitive_data[m_curr_primitive++] = data;
		}

		PrimitiveData GetPrimitiveData(u32 primitive_index) const {
			return m_per_primitive_data[primitive_index];
		}

		void ClearPrimitiveData() {
			m_curr_primitive = 0;
		}

		~Pipeline() {}

		static constexpr u32 VERTEX_COUNT_PER_PRIMITIVE = vertex_count_per_primitive(Prim);

	private :
		VertexBuffer<Vert> m_buffer;
		Shader m_shader;
		PrimitiveData m_per_primitive_data[MAX_PRIMITIVE_COUNT];
		u32 m_curr_primitive;
	};
}