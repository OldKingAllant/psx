#pragma once

#include <GL/glew.h>

#include <map>
#include <string>
#include <string_view>
#include <vector>
#include <optional>

#include <fmt/format.h>

#include <common/Errors.hpp>

namespace psx::video {
	class Shader {
	public :
		Shader(std::string const& location, std::string const& name);

		template <typename UniformTy>
		void UpdateUniform(std::string_view name, UniformTy value) {
			if (!m_uniforms.contains(std::string(name))) {
				GLuint uniform_loc = glGetUniformLocation(m_program_id, name.data());

				if (uniform_loc == GL_INVALID_INDEX) {
					fmt::println("[RENDERER] Shader {} with id {}, cannot find uniform {}",
						m_shader_name, m_program_id, name);
					return;
				}

				m_uniforms.insert(std::pair{ std::string(name), uniform_loc });
			}

			GLuint uniform_loc = m_uniforms.find(std::string(name))->second;

			if constexpr (std::is_same_v<UniformTy, float>) {
				glUniform1f(uniform_loc, value);
			}
			else if constexpr (std::is_same_v<UniformTy, int>) {
				glUniform1i(uniform_loc, value);
			}
			else if constexpr (std::is_same_v<UniformTy, unsigned int>) {
				glUniform1ui(uniform_loc, value);
			}
			else if constexpr (std::is_same_v<UniformTy, std::array<unsigned int, 2>>) {
				glUniform2ui(uniform_loc, value[0], value[1]);
			}
			else if constexpr (std::is_same_v<UniformTy, std::array<unsigned int, 3>>) {
				glUniform3ui(uniform_loc, value[0], value[1], value[2]);
			}
			else if constexpr (std::is_same_v<UniformTy, bool>) {
				glUniform1i(uniform_loc, value);
			}
			else {
				fmt::println("[RENDERER] Invalid uniform type");
				error::DebugBreak();
			}
		}

		void BindUbo(GLuint bind_point, std::string_view ubo_name);

		void BindProgram();

		std::optional<uint32_t> UniformLocation(std::string const& name);

		void SetLabel(std::string_view label);

		~Shader();

	private :
		void CompileShader(GLenum id, std::vector<char> const& src);
		void LinkShaders(GLenum vert, GLenum frag);

	private :
		std::string m_shaderloc;
		std::string m_shader_name;

		GLuint m_program_id;
		
		std::map<std::string, GLint> m_uniforms;
		std::map<std::string, GLuint> m_ubos;
	};
}