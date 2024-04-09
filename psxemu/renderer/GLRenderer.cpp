#include "GLRenderer.hpp"

namespace psx::video {
	Renderer::Renderer() :
		m_vram{} {}

	void Renderer::BlitEnd() {
		m_vram.Upload();
	}

	void Renderer::VBlank() {
		m_vram.Upload();
	}

	Renderer::~Renderer() {
		//
	}
}