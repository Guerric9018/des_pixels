#pragma once
#include <glad/glad.h>
#include <string_view>
#include <utility>
#include "data.hpp"


class ShaderStage
{
	GLuint id;

public:
	enum stage {
		Vertex   = GL_VERTEX_SHADER  ,
		Fragment = GL_FRAGMENT_SHADER,
	};

	ShaderStage(std::string_view code, stage stg)
	{
		id = glCreateShader(stg);
		const char *start = code.data();
		glShaderSource(id, 1, &start, NULL);
		glCompileShader(id);
		int status;
		glGetShaderiv(id, GL_COMPILE_STATUS, &status);
		if (status != GL_TRUE) {
			char buf[256];
			GLsizei len;
			glGetShaderInfoLog(id, sizeof buf, &len, buf);
			std::cerr << "[OpenGL] "
				  << (stg == Vertex ? "Vertex": "Fragment")
				  << " shader stage compilation failed: "
				  << buf;
		}
	}

	ShaderStage(ShaderStage &&other) : id(std::exchange(other.id, 0)) {}

	~ShaderStage()
	{
		glDeleteShader(id);
	}

	GLuint get() const { return id; }
};

class Shader
{
	GLuint id;

public:
	Shader(ShaderStage &&vertex, ShaderStage &&fragment)
	{
		id = glCreateProgram();
		glAttachShader(id, vertex.get());
		glAttachShader(id, fragment.get());
		glLinkProgram(id);
	}

	Shader(Shader &&other)
		: id(std::exchange(other.id, 0))
	{
	}

	~Shader()
	{
		glDeleteProgram(id);
	}

	void bind() const
	{
		glUseProgram(id);
	}

	template <typename T>
	inline void set(std::string_view name, T const &value) const
	{
		static_assert(false, "set unimplemented for this type");
	}

};

template <>
inline void Shader::set<float>(std::string_view name, float const &value) const
{
	const auto loc = glGetUniformLocation(id, name.data());
	glUniform1f(loc, value);
}

template <>
inline void Shader::set<vec4<float>>(std::string_view name, vec4<float> const &value) const
{
	const auto loc = glGetUniformLocation(id, name.data());
	glUniform4f(loc, value.x, value.y, value.z, value.w);
}
