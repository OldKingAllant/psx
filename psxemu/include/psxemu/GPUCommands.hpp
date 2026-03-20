#pragma once

#include <common/Defs.hpp>
#include <common/Macros.hpp>

namespace psx {
	enum class CommandType {
		MISC,
		POLYGON,
		LINE, 
		RECTANGLE,
		VRAM_BLIT,
		CPU_VRAM_BLIT,
		VRAM_CPU_BLIT,
		ENV
	};

	/*
	bit number   value   meaning
	31-29        001    polygon render
	  28         1/0    gouraud / flat shading
	  27         1/0    4 / 3 vertices
	  26         1/0    textured / untextured
	  25         1/0    semi-transparent / opaque
	  24         1/0    raw texture / modulation
	 23-0        rgb    first color value.
	*/
	struct PolygonCmd {
		u32 cmd;
		PolygonCmd(u32 value) : cmd(value) {}

		FORCE_INLINE u32 color() const { return (cmd) & 0xFF'FF'FF; }
		FORCE_INLINE bool is_raw_texture() const { return (cmd >> 24) & 1; }
		FORCE_INLINE bool is_semi_transparent() const { return (cmd >> 25) & 1; }
		FORCE_INLINE bool is_textured() const { return (cmd >> 26) & 1; }
		FORCE_INLINE bool is_quad() const { return (cmd >> 27) & 1; }
		FORCE_INLINE bool is_gouraud() const { return (cmd >> 28) & 1; }

		FORCE_INLINE u32 get_flags() const { return (cmd >> 24) & 0x1F; }
	};

	/*
	bit number   value   meaning
  31-29        011    rectangle render
  28-27        sss    rectangle size
    26         1/0    textured / untextured
    25         1/0    semi-transparent / opaque
    24         1/0    raw texture / modulation
   23-0        rgb    first color value.

	0 (00)      variable size
	1 (01)      single pixel (1x1)
	2 (10)      8x8 sprite
	3 (11)      16x16 sprite
	*/
	struct RectCmd {
		u32 cmd;
		RectCmd(u32 value) : cmd(value) {}

		static constexpr u32 RECT_SIZES[] = { u32(-1), 1, 8, 16 };

		FORCE_INLINE u32 color() const { return (cmd) & 0xFF'FF'FF; }
		FORCE_INLINE bool is_raw_texture() const { return (cmd >> 24) & 1; }
		FORCE_INLINE bool is_semi_transparent() const { return (cmd >> 25) & 1; }
		FORCE_INLINE bool is_textured() const { return (cmd >> 26) & 1; }
		FORCE_INLINE std::pair<u32, u32> get_size() {
			auto size_descriptor = (cmd >> 27) & 0x3;
			return { RECT_SIZES[size_descriptor], RECT_SIZES[size_descriptor] };
		}
		
		FORCE_INLINE u32 get_flags() const { return (cmd >> 24) & 0x7; }
	};

	/*
	bit number   value   meaning
    31-29        010    line render
    28         1/0    gouraud / flat shading
    27         1/0    polyline / single line
    25         1/0    semi-transparent / opaque
    23-0        rgb    first color value.
	*/
	struct LineCmd {
		u32 cmd;
		LineCmd(u32 value) : cmd(value) {}

		FORCE_INLINE u32 color() const { return (cmd) & 0xFF'FF'FF; }
		FORCE_INLINE bool is_semi_transparent() const { return (cmd >> 25) & 1; }
		FORCE_INLINE bool is_polyline() const { return (cmd >> 27) & 1; }
		FORCE_INLINE bool is_gouraud() const { return (cmd >> 28) & 1; }

		FORCE_INLINE u32 get_flags() const { 
			auto semi_transparent = u32(is_semi_transparent()) & 1;
			auto polyline = u32(is_polyline()) & 1;
			auto gouraud = u32(is_gouraud()) & 1;
			return (gouraud << 2) | (polyline << 1) | semi_transparent;
		}
	};

	/*
	Color      xxBBGGRR               - optional, only present for gouraud shading
	Vertex     YYYYXXXX               - required, two signed 16 bits values
	UV         ClutVVUU or PageVVUU   - optional, only present for textured polygons
	*/
	struct ColorAttribute {
		u32 attribute;
		ColorAttribute(u32 value) : attribute(value) {}

		FORCE_INLINE u32 rgb() const { return (attribute) & 0xFF'FF'FF; }

		FORCE_INLINE u32 r() const { return (attribute) & 0xFF; }
		FORCE_INLINE u32 g() const { return (attribute >> 8) & 0xFF; }
		FORCE_INLINE u32 b() const { return (attribute >> 16) & 0xFF; }
	};

	struct PositionAttribute {
		u32 attribute;
		PositionAttribute(u32 value) : attribute(value) {}

		FORCE_INLINE i32 x() const { return sign_extend<i32, 10>(attribute & 0xFFFF); };
		FORCE_INLINE i32 y() const { return sign_extend<i32, 10>((attribute >> 16) & 0xFFFF); };
	};

	struct UVAttribute {
		//UV         ClutVVUU or PageVVUU   - optional, only present for textured polygons
		u32 attribute;
		UVAttribute(u32 value) : attribute(value) {}

		FORCE_INLINE u32 u() const { return (attribute) & 0xFF; }
		FORCE_INLINE u32 v() const { return (attribute >> 8) & 0xFF; }

		FORCE_INLINE u32 clut_or_page() const { return (attribute >> 16) & 0xFFFF; }
	};

	struct SizeAttribute {
		u32 attribute;
		SizeAttribute(u32 value) : attribute(value) {}

		FORCE_INLINE u32 sizex() const { return attribute & 0xFFFF; };
		FORCE_INLINE u32 sizey() const { return (attribute >> 16) & 0xFFFF; };
	};
}