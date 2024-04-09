#pragma once

#include <common/Defs.hpp>
#include <common/Macros.hpp>

#include "Buffer.hpp"

#include <vector>
#include <concepts>

namespace psx::video {
	u32 CreateVertexArray();
	void DeleteVertexArray(u32& id);

	enum class VertexAttributeType {
		BYTE, UNSIGNED_BYTE, SHORT,
		UNSIGNED_SHORT, INT, UNSIGNED_INT,
		FLOAT
	};

	u32 VertexAttributeToOpenGL(VertexAttributeType attr);

	void MakeCurrentVertexArray(u32 id);
	void MakeCurrentArrayBuffer(u32 id);

	static constexpr u32 VERTEX_ATTRIB_SIZES[] = {
		1, 1, 1, 1, 1, 1, 1
	};

	struct VertexAttribute {
		VertexAttributeType type;
		std::ptrdiff_t offset;
	};

	void SetVertexAttributes(std::vector<VertexAttribute> const& attributes, u32 vert_size);

	template <typename Vert>
	concept Vertex = requires(Vert v) {
		{ v.attributes() } -> std::same_as<std::vector<VertexAttribute>>;
	};

	struct HostVertex2D {
#pragma pack(push, 1)
		float x, y;
		float u, v;
#pragma pack(pop)

		std::vector<VertexAttribute> attributes() const {
			return {
				VertexAttribute{ VertexAttributeType::FLOAT, (u8*)&x - (u8*)this },
				VertexAttribute{ VertexAttributeType::FLOAT, (u8*)&y - (u8*)this },
				VertexAttribute{ VertexAttributeType::FLOAT, (u8*)&u - (u8*)this },
				VertexAttribute{ VertexAttributeType::FLOAT, (u8*)&v - (u8*)this },
			};
		}
	};

	template <Vertex Vert>
	class VertexBuffer {
	public :
		VertexBuffer(u32 max_vertices) :
			m_max_verts{max_vertices},
			m_curr_verts{}, 
			m_buf{true, BufferMap::READWRITE, sizeof(Vert) * (max_vertices) },
			m_vertex_arr{} {
			m_vertex_arr = CreateVertexArray();
			MakeCurrentArrayBuffer(m_buf.BufID());
			MakeCurrentVertexArray(m_vertex_arr);

			Vert dummy{};

			auto attribs = dummy.attributes();

			SetVertexAttributes(attribs, sizeof(Vert));

			MakeCurrentVertexArray(0);
			MakeCurrentArrayBuffer(0);
		}

		bool PushVertex(Vert new_vert) {
			if (m_curr_verts >= m_max_verts)
				return false;

			u64 off = m_curr_verts * sizeof(Vert);
			auto ptr = m_buf.Get();

			std::memcpy((u8*)ptr + off, (const void*)&new_vert,
				sizeof(Vert));

			m_curr_verts++;

			m_buf.Flush();

			return true;
		}

		u32 VertexCount() const {
			return m_curr_verts;
		}

		u32 MaxVertexCount() const {
			return m_max_verts;
		}

		void Bind() const {
			MakeCurrentVertexArray(m_vertex_arr);
		}

		void Unbind() const {
			MakeCurrentVertexArray(0);
		}

		~VertexBuffer() {
			DeleteVertexArray(m_vertex_arr);
		}

	private :
		u32 m_max_verts;
		u32 m_curr_verts;
		Buffer m_buf;
		u32 m_vertex_arr;
	};
}