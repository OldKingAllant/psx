#pragma once

#include <psxemu/renderer/SdlWindow.hpp>

#include <vector>

class DisplayWindow : public psx::video::SdlWindow {
public :
	using u32 = psx::u32;
	using u16 = psx::u16;
	using u8 = psx::u8;
	using Rect = psx::video::Rect;

	DisplayWindow(std::string name, psx::video::Rect size, std::string blit_loc,
		std::string blit16_name, std::string blit24_name,  bool reuse_ctx, bool resize, 
		bool enable_debug);

	~DisplayWindow();

	void SetTextureWindow24(u32 start_x, u32 start_y, Rect window_size, Rect texture_size);
	void Blit24(uint32_t texture_id);

private :
	psx::video::Shader* m_blit24_shader;
	GLuint m_24bit_tex;
	std::vector<u8> m_24bit_buf;
	std::vector<u16> m_temp_buf;
	GLuint m_ssbo_buf;
};