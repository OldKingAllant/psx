#pragma once

#include <GL/glew.h>

#include <map>
#include <string>
#include <string_view>
#include <vector>

#include <fmt/format.h>

namespace psx::video {
	class Shader {
	public :
		Shader(std::string const& location, std::string const& name);

		template <typename UniformTy, typename InputTy>
		void UpdateUniform(InputTy&& value, std::string_view name) {
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

			auto converted = static_cast<UniformTy>(value);

			if constexpr (std::is_same_v<UniformTy, float>) {
				glUniform1f(uniform_loc, converted);
			}
			else if constexpr (std::is_same_v<UniformTy, int>) {
				glUniform1i(uniform_loc, converted);
			}
			else if constexpr (std::is_same_v<UniformTy, unsigned int>) {
				glUniform1ui(uniform_loc, converted);
			}
			else {
				fmt::println("[RENDERER] Invalid uniform type");
			}
		}

		void BindUbo(GLuint bind_point, std::string_view ubo_name);

		void BindProgram();

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