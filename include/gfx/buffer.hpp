#include <glad/glad.h>
#include <span>
#include <type_traits>
#include <concepts>
#include <utility>

template <typename T>
struct type2gl {
	static_assert(false, "type has no valid GL enum value");
};

template <>
struct type2gl<float> : std::integral_constant<GLenum, GL_FLOAT> {};

template <typename T>
concept buffer_description = requires {
	typename T::element_type;
	typename T::attrib_type;
	typename T::vertex_type;
	T::element_count;
	T::location;
	// { T::element_count } -> std::same_as<GLint>;
	// { T::location } -> std::same_as<GLuint>;
};

class GlBuffer
{
	GLuint id;

public:
	GlBuffer()
	{
		glGenBuffers(1, &id);
	}

	GlBuffer(GlBuffer &&other) : id(std::exchange(other.id, 0)) {}

	~GlBuffer()
	{
		glDeleteBuffers(1, &id);
	}

	void bind(GLenum target) const
	{
		glBindBuffer(target, id);
	}

	void leak()
	{
		id = 0;
	}
};

struct VertexBuffer : GlBuffer
{
	template <buffer_description descr>
	VertexBuffer(std::span<typename descr::attrib_type> data, descr)
	{
		bind(GL_ARRAY_BUFFER);
		glBufferData(GL_ARRAY_BUFFER, data.size_bytes(), data.data(), GL_STATIC_DRAW);
		glVertexAttribPointer(
			descr::location,
			descr::element_count,
			type2gl<typename descr::element_type>::value,
			GL_FALSE,
			sizeof(typename descr::vertex_type),
			nullptr
		);
		glEnableVertexAttribArray(descr::location);
	}
};

struct IndexBuffer : GlBuffer
{
	IndexBuffer(std::span<GLuint> data)
	{
		bind(GL_ELEMENT_ARRAY_BUFFER);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, data.size_bytes(), data.data(), GL_STATIC_DRAW);
	}
};

class VertexArray
{
	GLuint id;

public:
	VertexArray()
	{
		glGenVertexArrays(1, &id);
		bind();
	}

	VertexArray(VertexArray &&other) : id(std::exchange(other.id, 0)) {}

	~VertexArray()
	{
		glDeleteVertexArrays(1, &id);
	}

	void bind() const
	{
		glBindVertexArray(id);
	}
};

