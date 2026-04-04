#pragma once

#include <common/Defs.hpp>
#include <common/Macros.hpp>

namespace psx {
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

	enum class CommandRegister {
		GP0,
		GP1
	};

	enum class GP1CommandType {
		RESET,
		RESET_CMD_FIFO,
		IRQ_ACK,
		DISPLAY_ENABLE,
		DMA_DIRECTION,
		DISPLAY_AREA_START,
		HORIZONTAL_DISPLAY_RANGE,
		VERTICAL_DISPLAY_RANGE,
		DISPLAY_MODE,
		SET_VRAM_SIZE,
		READ_GPU_REGISTER = 0x10
	};

	enum class GP0CommandType {
		MISC,
		POLYGON,
		LINE,
		RECTANGLE,
		VRAM_BLIT,
		CPU_VRAM_BLIT,
		VRAM_CPU_BLIT,
		ENV,
		POLYLINE_END = 0xDEAD //Construct for debugging
	};

	enum class MiscCommandType {
		NOP = 0x00,
		CLEAR_CACHE = 0x01,
		QUICK_FILL = 0x02,
		NOP_FIFO = 0x03
	};

	enum class EnvCommandType {
		TEXTURE_PAGE    = 0xE1,
		TEXTURE_WINDOW  = 0xE2,
		SET_DRAW_TOP    = 0xE3,
		SET_DRAW_BOTTOM = 0xE4,
		SET_DRAW_OFFSET = 0xE5,
		MASK_BIT        = 0xE6
	};

	struct DisplayEnableCmd {
#pragma pack(push, 1)
		bool display_on : 1;
#pragma pack(pop)
	};

	struct DmaDirectionCmd {
#pragma pack(push, 1)
		u8 direction : 2;
#pragma pack(pop)
	};

	union DisplayAreaStartCmd {
		struct {
#pragma pack(push, 1)
			u32 x : 10;
			u32 y : 10;
#pragma pack(pop)
		};

		u32 reg;
	};

	union HorizontalDisplayRangeCmd {
		struct {
#pragma pack(push, 1)
			u32 x1 : 12;
			u32 x2 : 12;
#pragma pack(pop)
		};

		u32 reg;
	};

	union VerticalDisplayRangeCmd {
		struct {
#pragma pack(push, 1)
			u32 y1 : 10;
			u32 y2 : 10;
#pragma pack(pop)
		};

		u32 reg;
	};

	union DisplayModeCmd {
		struct {
#pragma pack(push, 1)
			u32 horizontal_resolution_1 : 2;
			u32 vertical_resolution : 1;
			u32 video_mode : 1;
			u32 color_depth : 1;
			u32 vertical_interlace : 1;
			u32 horizontal_resolution_2 : 1;
			u32 flip_screen_x_axis : 1;
#pragma pack(pop)
		};

		u32 reg;
	};

	struct SetVramSizeCmd {
#pragma pack(push, 1)
		u32 two_mbytes : 1;
#pragma pack(pop)
	};

	struct GP1Command {
		GP1CommandType type;

		union {
			u32 cmd;
			DisplayEnableCmd disp_enable;
			DmaDirectionCmd dma_dir;
			DisplayAreaStartCmd disp_start;
			HorizontalDisplayRangeCmd hoz_disp_range;
			VerticalDisplayRangeCmd vert_disp_range;
			DisplayModeCmd disp_mode;
			SetVramSizeCmd set_vram_size;
		};
	};

	struct QuickFillCmd {
#pragma pack(push, 1)
		u32 r : 8;
		u32 g : 8;
		u32 b : 8;
#pragma pack(pop)
	};

	struct QuickFillParams {
		u32 x;
		u32 y;
		u32 w;
		u32 h;
	};

	struct MiscCommand {
		MiscCommandType type;

		union {
			u32 cmd;
			QuickFillCmd quick_fill;
		};
	};

	struct VramVramBlitParams {
		u32 src_x;
		u32 src_y;
		u32 dst_x;
		u32 dst_y;
		u32 w;
		u32 h;
	};

	struct CpuVramBlitParams {
		u32 dst_x;
		u32 dst_y;
		u32 w;
		u32 h;
	};

	struct VramCpuBlitParams {
		u32 src_x;
		u32 src_y;
		u32 w;
		u32 h;
	};

	struct TexturePageCmd {
#pragma pack(push, 1)
		u32 x_base : 4;
		u32 y_base_1 : 1;
		u32 semi_transparency : 2;
		u32 texpage_colors : 2;
		u32 enable_dither : 1;
		u32 draw_to_display : 1;
		u32 y_base_2 : 1;
		u32 texture_x_flip : 1;
		u32 texture_y_flip : 1;
#pragma pack(pop)
	};

	struct TextureWindowCmd {
#pragma pack(push, 1)
		u32 mask_x : 5;
		u32 mask_y : 5;
		u32 offset_x : 5;
		u32 offset_y : 5;
#pragma pack(pop)
	};

	struct DrawingAreaCmd {
#pragma pack(push, 1)
		u32 x_coord : 10;
		u32 y_coord : 9;
#pragma pack(pop)
	};

	struct DrawingOffsetCmd {
#pragma pack(push, 1)
		i32 x_offset : 11;
		i32 y_offset : 11;
#pragma pack(pop)
	};

	struct MaskBitCmd {
#pragma pack(push, 1)
		u32 set_mask : 1;
		u32 check_mask : 1;
#pragma pack(pop)
	};

	struct EnvCommand {
		EnvCommandType type;

		union {
			TexturePageCmd texpage;
			TextureWindowCmd texwindow;
			DrawingAreaCmd draw_area;
			DrawingOffsetCmd draw_offset;
			MaskBitCmd mask_bit;
			u32 cmd;
		};
	};

	struct PolylineEndMarker {};

	struct GP0Command {
		GP0CommandType type;

		union {
			MiscCommand misc;
			PolygonCmd polygon;
			RectCmd rect;
			LineCmd line;
			EnvCommand env;
			PolylineEndMarker end_marker{};
		};
	};

	struct VertexParams {
		i32 x, y;
		u32 u, v;
		ColorAttribute color;
		u32 clut_page;
	};

	struct RenderingParams {
		VertexParams vertices[4];
		u8 vertex_count;
		bool semi_transparent;
		u8 transparency_type;
	};

	union CmdParams {
		QuickFillParams quick_fill;
		VramVramBlitParams vram_vram_blit;
		CpuVramBlitParams cpu_vram_blit;
		VramCpuBlitParams vram_cpu_blit;
		RenderingParams rendering;
	};

	struct GPUCommand {
		CommandRegister reg;
		u32 value;
		u64 frame_of_recording;
		bool from_prev_frame;
		u64 start_index = UINT64_MAX;

		union {
			GP1Command gp1;
			GP0Command gp0;
		};

		CmdParams params;
	};

	
}