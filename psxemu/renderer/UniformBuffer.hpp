#pragma once

#include <common/Defs.hpp>

#include <concepts>
#include <string_view>

namespace psx::video {
	uint32_t CreateUniformBuffer(uint32_t size);
	void DeleteUniformBuffer(uint32_t ubo);
	void UnbindUniformBuffer(uint32_t ubo);
	void BindUniformBuffer(uint32_t ubo);
	void BindUniformBufferRange(uint32_t ubo, uint32_t bind_point, uint32_t off, uint32_t size);
	void UploadUniformBufferData(uint32_t off, uint32_t size, void* data);
	void UniformBufferLabel(std::string_view label, uint32_t buffer);

	template <typename Buf>
	class UniformBuffer {
	public :
		static_assert(std::is_standard_layout_v<Buf>, "Type must be POD");
		static_assert(std::is_default_constructible_v<Buf>, "Type must be default constructible");

		UniformBuffer() :
			buffer{}, m_ubo{}, m_bind_point{0xFFFF} {
			m_ubo = CreateUniformBuffer(sizeof(Buf));
		}

		~UniformBuffer() {
			Unbind();
			DeleteUniformBuffer(m_ubo);
		}

		Buf buffer{};

		void BindRange(uint32_t bind_point) {
			if (m_bind_point == bind_point)
				return;

			m_bind_point = bind_point;

			BindUniformBufferRange(m_ubo, m_bind_point, 0, sizeof(Buf));
		}

		void Bind() {
			BindUniformBuffer(m_ubo);
		}

		void Unbind() {
			UnbindUniformBuffer(m_ubo);
		}

		void SetLabel(std::string_view label) {
			UniformBufferLabel(label, m_ubo);
		}

		void Upload() {
			UploadUniformBufferData(0, sizeof(Buf), (void*)&buffer);
		}

	private :
		uint32_t m_ubo;
		uint32_t m_bind_point;
	};
}