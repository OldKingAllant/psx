#include "Shader.hpp"

#include <fmt/format.h>

#include <concepts>
#include <filesystem>
#include <fstream>

namespace psx::video {
	Shader::Shader(std::string const& location, std::string const& name)
		: m_shaderloc{location}, m_shader_name{name}, 
		m_program_id{}, m_uniforms{}, m_ubos{} {
		auto vertex_shader = m_shaderloc + "/" +
			m_shader_name + ".vertex";
		auto frag_shader = m_shaderloc + "/" +
			m_shader_name + ".fragment";

		if (!std::filesystem::exists(vertex_shader) ||
			!std::filesystem::exists(frag_shader))
			throw std::runtime_error("Shader creation failed, not found");

		auto vert_sz = std::filesystem::file_size(vertex_shader);
		auto frag_sz = std::filesystem::file_size(frag_shader);

		std::vector<char> vert_source = {};
		std::vector<char> frag_source = {};

		vert_source.resize(vert_sz);
		frag_source.resize(frag_sz);

		std::fstream input{};

		auto vert_ptr = vert_source.data();

		input.open(vertex_shader, std::ios::in | std::ios::binary);

		if (!input.is_open())
			throw std::runtime_error("Open failed");

		input.read(vert_ptr, vert_sz);
		
		input.close();

		/////

		input.open(frag_shader, std::ios::in | std::ios::binary);

		if (!input.is_open())
			throw std::runtime_error("Open failed");

		auto frag_ptr = frag_source.data();

		input.read(frag_ptr, frag_sz);

		input.close();

		GLenum vert_id = glCreateShader(GL_VERTEX_SHADER);
		GLenum frag_id = glCreateShader(GL_FRAGMENT_SHADER);

		m_program_id = glCreateProgram();

		CompileShader(vert_id, vert_source);
		CompileShader(frag_id, frag_source);

		LinkShaders(vert_id, frag_id);
	} 

	void Shader::CompileShader(GLenum id, std::vector<char> const& src) {
		const char* data = src.data();
		GLint len = (GLint)src.size();

		glShaderSource(
			id, 1, &data, &len
		);
		glCompileShader(id);

		GLint compile_status{};

		glGetShaderiv(id, GL_COMPILE_STATUS, &compile_status);

		if (compile_status == GL_FALSE) {
			std::string err{};
			GLsizei effective_size{};
			err.reserve(1024);

			glGetShaderInfoLog(id, 1024, &effective_size, err.data());

			std::string copy{ err.data(), err.data() + effective_size};

			fmt::println("[RENDERER] Shader {} compilation failed :", m_shader_name);
			fmt::println("{}", copy);
			throw std::runtime_error("Shader creation failed, compilation failed");
		}
	}

	void Shader::LinkShaders(GLenum vert, GLenum frag) {
		glAttachShader(m_program_id, vert);
		glAttachShader(m_program_id, frag);

		glLinkProgram(m_program_id);

		GLint status = 0;

		glGetProgramiv(m_program_id, GL_LINK_STATUS, &status);

		if (status == GL_FALSE) {
			std::string err{};
			GLsizei effective_size{};
			err.reserve(1024);

			glGetProgramInfoLog(m_program_id, 1024, &effective_size, err.data());

			std::string copy{ err.data(), err.data() + effective_size };

			fmt::println("[RENDERER] Shader {} compilation failed :", m_shader_name);
			fmt::println("{}", copy);
			throw std::runtime_error("Shader creation failed, link failed");
		}

		glValidateProgram(m_program_id);
	}

	void Shader::BindUbo(GLuint bind_point, std::string_view ubo_name) {
		if (!m_ubos.contains(std::string(ubo_name))) {
			GLuint ubo_index = glGetUniformBlockIndex(m_program_id, ubo_name.data());

			if (ubo_index == GL_INVALID_INDEX) {
				fmt::println("[RENDERER] Shader {} with id {}, cannot find UBO {}",
					m_shader_name, m_program_id, ubo_name);
				return;
			}

			m_ubos.insert(std::pair{ std::string(ubo_name), ubo_index });
		}

		GLuint ubo_index = m_ubos.find(std::string(ubo_name))->second;

		glBindBufferBase(GL_UNIFORM_BUFFER, bind_point,
			ubo_index);
	}

	std::optional<uint32_t> Shader::UniformLocation(std::string const& name) {
		if (!m_uniforms.contains(std::string(name))) {
			GLuint uniform_loc = glGetUniformLocation(m_program_id, name.c_str());

			if (uniform_loc == GL_INVALID_INDEX) {
				fmt::println("[RENDERER] Shader {} with id {}, cannot find uniform {}",
					m_shader_name, m_program_id, name);
				return std::nullopt;
			}

			m_uniforms.insert(std::pair{ std::string(name), uniform_loc });
		}

		GLuint uniform_loc = m_uniforms.find(std::string(name))->second;

		return uniform_loc;
	}

	void Shader::BindProgram() {
		glUseProgram(m_program_id);
	}

	Shader::~Shader() {
		glDeleteProgram(m_program_id);
	}
}