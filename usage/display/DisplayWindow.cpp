#include "DisplayWindow.hpp"

#include <fmt/format.h>
#include <SDL2/SDL.h>

DisplayWindow::DisplayWindow(std::string name, psx::video::Rect size, std::string blit_loc,
	std::string blit16_name, std::string blit24_name, bool reuse_ctx, bool resize,
	bool enable_debug) : SdlWindow(name, size, blit_loc, blit16_name, 
		reuse_ctx, resize, enable_debug) {
	m_blit24_shader = new psx::video::Shader(blit_loc, blit24_name);
	m_blit24_shader->SetLabel(fmt::format("window_{}_blit24_shader", name));

	m_temp_buf.resize(1024ULL * 512);

	////////////////////////

	glGenBuffers(1, &m_ssbo_buf);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, m_ssbo_buf);
	glBufferData(GL_PIXEL_PACK_BUFFER, 1024ULL * 512 * 2,
		nullptr, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, m_ssbo_buf);
}

DisplayWindow::~DisplayWindow() {
	delete m_blit24_shader;
	glDeleteBuffers(1, &m_ssbo_buf);
}

void DisplayWindow::SetTextureWindow24(u32 start_x, u32 start_y, Rect window_size, Rect texture_size) {
	m_vert_buf->Clear();

	float w = float(window_size.w) / texture_size.w;
	float h = float(window_size.h) / texture_size.h;

	float xoff = float(start_x) / texture_size.w;
	float yoff = float(start_y) / texture_size.h;
	float endx = xoff + w;
	float endy = yoff + h;

	using psx::video::HostVertex2D;

	m_vert_buf->PushVertex(HostVertex2D{ xoff, yoff, 0.0, 0.0  });
	m_vert_buf->PushVertex(HostVertex2D{ xoff, yoff, 1.0, 1.0  });
	m_vert_buf->PushVertex(HostVertex2D{ xoff, yoff, 0.0, 1.0 });
	
	m_vert_buf->PushVertex(HostVertex2D{ xoff, yoff, 0.0, 0.0 });
	m_vert_buf->PushVertex(HostVertex2D{ xoff, yoff, 1.0, 0.0 });
	m_vert_buf->PushVertex(HostVertex2D{ xoff, yoff, 1.0, 1.0 });

	m_blit24_shader->BindProgram();
	m_blit24_shader->UpdateUniform("resolution_x", float(window_size.w));
	m_blit24_shader->UpdateUniform("resolution_y", float(window_size.h));
}

void DisplayWindow::Blit24(uint32_t texture_id) {
	SDL_GL_MakeCurrent((SDL_Window*)m_win, m_gl_ctx);

	if (!m_blit || !m_vert_buf)
		throw std::runtime_error("Window is not ready for blit ops");

	int curr_viewport[4] = {};
	glGetIntegerv(GL_VIEWPORT, curr_viewport);

	glViewport(0, 0, (GLsizei)m_size.w, (GLsizei)m_size.h);

	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	m_blit24_shader->BindProgram();
	m_vert_buf->Bind();

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture_id);

	glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA,
		GL_UNSIGNED_SHORT_1_5_5_5_REV_EXT,
		std::bit_cast<void*>(m_temp_buf.data()));
	
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, 1024ULL * 512 * 2,
		std::bit_cast<void*>(m_temp_buf.data()));
	glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);


	////////////////////////////////////////////////

	glDrawArrays(GL_TRIANGLES, 0, 6);

	m_vert_buf->Unbind();

	glViewport(curr_viewport[0], curr_viewport[1], curr_viewport[2], curr_viewport[3]);
}
