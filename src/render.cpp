#include "gfx/render.hpp"


Render::Render(Window &&window)
	: reset(0), window(std::move(window))
{
}

bool Render::clear()
{
	glClear(GL_COLOR_BUFFER_BIT);
	glfwPollEvents();
	commands.resize(reset);
	return !glfwWindowShouldClose(window.handle);
}

void Render::do_draw(command const &cmd)
{
	cmd.shader->bind();
	cmd.shader->set("color", cmd.color);
	cmd.va->bind();
	size_t drawn = 0;
	const char *at = cmd.data;
	GLsizei attrib_count = (cmd.primitive == GL_QUADS) ? 6: 2;
	GLenum primitive = (cmd.primitive == GL_QUADS) ? GL_TRIANGLES: GL_LINES;
	size_t stride = cmd.vb_cnt * cmd.attrib_size;
	for (; cmd.inst_cnt - drawn > cmd.vb_cnt; drawn += cmd.vb_cnt, at += stride) {
		cmd.vb->update(std::span{at, at+stride});
		glDrawElementsInstanced(primitive, attrib_count, GL_UNSIGNED_INT, nullptr, cmd.vb_cnt);
	}
	size_t rem_inst = cmd.inst_cnt - drawn;
	cmd.vb->update(std::span{at, at+ rem_inst*cmd.attrib_size});
	glDrawElementsInstanced(primitive, attrib_count, GL_UNSIGNED_INT, nullptr, rem_inst);
}

void Render::draw()
{
	for (const auto &cmd : commands)
		do_draw(cmd);
	glfwSwapBuffers(window.handle);
}

void Render::keep()
{
	reset = commands.size();
}

void Render::removeAll()
{
	reset = 0;
}

