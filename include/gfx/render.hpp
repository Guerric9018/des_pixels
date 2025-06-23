#pragma once
#include <vector>
#include <glad/glad.h>
#include "data.hpp"
#include "gfx.hpp"
#include "gfx/shader.hpp"
#include "gfx/buffer.hpp"


class Render
{
	struct command
	{
		Shader const *shader;
		VertexArray const *va;
		VertexBuffer const *vb;
		size_t vb_cnt;
		const char *data;
		size_t attrib_size;
		size_t inst_cnt;
		vec4<float> color;
		GLenum primitive;
	};

	std::vector<command> commands;
	size_t reset;
	Window window;

	void do_draw(command const& cmd);
public:
	Render(Window &&window);

	template <buffer_description D>
	struct draw_context
	{
		Shader const &shader;
		VertexArray const &va;
		VertexBuffer const &vb;
		size_t vb_cnt;
	};

	template <buffer_description D>
	void submit(
		draw_context<D> ctx,
		std::span<const typename D::attrib_type> data,
		vec4<float> color,
		GLenum primitive
	)
	{
		commands.emplace_back(command{
			&ctx.shader,
			&ctx.va,
			&ctx.vb,
			ctx.vb_cnt,
			reinterpret_cast<const char*>(data.data()),
			sizeof(typename D::attrib_type),
			data.size(),
			color,
			primitive,
		});
	}

	bool clear();
	void draw();
	void keep();
	void removeAll();
};


