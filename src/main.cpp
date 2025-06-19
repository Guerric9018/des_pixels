#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <iostream>
#include <cassert>
#include <vector>
#include <cstdint>
#include "data.hpp"
#include "gfx.hpp"
#include "gfx/shader.hpp"
#include "gfx/buffer.hpp"

vec2<float> lerp(vec2<float> a, vec2<float> b, float t)
{
	return a + (b - a) * t;
}

float dist2(vec2<float> a, vec2<float> b)
{
	const auto delta = b - a;
	return delta.x * delta.x + delta.y * delta.y;
}

struct Mesh {
	std::vector<vec2<float>> vert;
	std::vector<std::pair<size_t, size_t>> edge;
};

size_t bit(size_t b)
{
	return size_t{1} << b;
}

Mesh buildShapes(Clusters& clusters, size_t width, size_t height)
{
	const auto index = [=] (size_t x, size_t y) { return x + y * width; };
	static const size_t ARITY = 3;

	size_t edge_node_end = 0;
	const size_t edge_node_max = 4*ARITY*width*height;
	std::vector<vec2<float>> nodes(edge_node_max);
	std::vector<std::pair<size_t, size_t>> edges;

	struct choice {
		size_t x;
		size_t y;
		size_t dir;
	};
	std::vector<choice> corners;

	struct offset_t {
		vec2<size_t> pixel;
		vec2<float > start;
	};

	offset_t offset[] = {
		{ { 0, size_t(-1) }, { +0.5f, -0.5f } },
		{ { size_t(-1), 0 }, { -0.5f, -0.5f } },
		{ { 0, size_t(+1) }, { -0.5f, +0.5f } },
		{ { size_t(+1), 0 }, { +0.5f, +0.5f } },
	};

	for (size_t y = 1; y < height - 1; ++y) {
		for (size_t x = 1; x < width - 1; ++x) {
			const auto current = clusters.repr(index(x, y));
			for (size_t o = 0; o < std::size(offset); ++o) {
				const auto neighbr = clusters.repr(index(
						x + offset[o].pixel.x,
						y + offset[o].pixel.y
				));
				if (current == neighbr)
					continue;
				const auto pos = vec2<float>(float(x), float(y));
				const auto start = pos + offset[o].start;
				const auto end   = pos + offset[(o+1) % 4].start;
				for (size_t edge_node = 0; edge_node < ARITY; ++edge_node) {
					const float t = float(edge_node+1) / float(ARITY+1);
					if (edge_node < ARITY-1) {
						edges.emplace_back(edge_node_end, edge_node_end+1);
					}
					nodes[edge_node_end++] = lerp(start, end, t);
				}
				nodes.emplace_back(start);
				nodes.emplace_back(end  );
				edges.emplace_back(nodes.size()-2, edge_node_end-ARITY);
				edges.emplace_back(nodes.size()-1, edge_node_end-1    );
				corners.push_back(choice{ x, y, o });
			}
		}
	}

	union_find node_map(nodes.size());
	static const float epsilon = 0.5f / float(ARITY + 1);
	// edge nodes
	for (size_t en1 = 0; en1 < edge_node_end; ++en1) {
		for (size_t en2 = en1 + 1; en2 < edge_node_end; ++en2) {
			if (dist2(nodes[en1], nodes[en2]) < epsilon) {
				node_map.unite(en1, en2);
			}
		}
	}

	assert(nodes.size() - edge_node_max == 2*corners.size());
	// corner nodes
	for (size_t cn1 = edge_node_max; cn1 < nodes.size(); ++cn1) {
		for (size_t cn2 = cn1 + 1; cn2 < nodes.size(); ++cn2) {
			if (dist2(nodes[cn1], nodes[cn2]) >= epsilon)
				continue;
			const auto is_end_1   = (cn1 - edge_node_max) % 2;
			const auto [x, y, o1] = corners[(cn1 - edge_node_max) / 2];
			const auto is_end_2   = (cn2 - edge_node_max) % 2;
			const auto o2         = corners[(cn2 - edge_node_max) / 2].dir;
			id_t current, counter1, counter2, diag;
			current = clusters.repr(index(x, y));
			switch ((is_end_1 + o1) % 4) {
				case 0:
					counter1 = clusters.repr(index(x  , y-1));
					counter2 = clusters.repr(index(x+1, y  ));
					diag = clusters.repr(index(x+1, y-1));
					break;
				case 1:
					counter1 = clusters.repr(index(x  , y-1));
					counter2 = clusters.repr(index(x-1, y  ));
					diag = clusters.repr(index(x-1, y-1));
					break;
				case 2:
					counter1 = clusters.repr(index(x-1, y  ));
					counter2 = clusters.repr(index(x  , y+1));
					diag = clusters.repr(index(x-1, y+1));
					break;
				case 3:
					counter1 = clusters.repr(index(x+1, y  ));
					counter2 = clusters.repr(index(x  , y+1));
					diag = clusters.repr(index(x+1, y+1));
					break;
			}
			if (current == diag && current != counter1 && current != counter2) {
				static const std::uint8_t can_fuse = 0b10010110;
				if ((can_fuse & bit(o1 << 1 | is_end_1) & bit(o2 << 1 | is_end_2)) == 0)
					continue;
			} else if (counter1 == counter2 && current != counter1 && diag != counter1) {
				static const std::uint8_t can_fuse = 0b01011010;
				if ((can_fuse & bit(o1 << 1 | is_end_1) & bit(o2 << 1 | is_end_2)) == 0)
					continue;
			}
			// merge
			node_map.unite(cn1, cn2);
		}
	}
}

int main(int argc, char **argv)
{
	if (argc != 3) {
		std::cout << "Usage: " << argv[0] << " <source> <target>\n";
		return 1;
	}
	Window window("Depixel", 1280, 720);

	Shader shader(
		ShaderStage(
			R"(#version 330 core
			layout(location = 0) in vec2 pos;
			void main() {
				gl_Position = vec4(pos, 0.0, 1.0);
			})", ShaderStage::Vertex),
		ShaderStage(
			R"(#version 330 core
			out vec4 frag_color;
			void main() {
				frag_color = vec4(0.8, 0.1, 0.2, 1.0);
			})", ShaderStage::Fragment)
	);
	shader.bind();

	struct attr0descr {
		using element_type = float;
		using attrib_type  = vec2<float>;
		using vertex_type  = vec2<float>;
		enum : GLint { element_count = 2 };
		enum : GLuint { location = 0 };
	};

	VertexArray va;
	{
		vec2<float> vertices[] = {
			{ -0.5f, -0.5f },
			{ +0.5f, -0.5f },
			{ +0.5f, +0.5f },
			{ -0.5f, +0.5f },
		};
		GLuint indices[] = {
			0, 1, 2,
			2, 3, 0,
		};
		VertexBuffer vb(vertices, attr0descr{});
		vb.leak();
		IndexBuffer ib(indices);
		ib.leak();
	}

	window.run([&] {
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
	});
	int width, height, channels;
	stbi_set_flip_vertically_on_load(false);
	auto pixels = stbi_load(argv[1], &width, &height, &channels, 0);
	assert(channels == 3);
	assert(width > 0 && height > 0);

	Clusters clusters(width, height, pixels);
	std::cout << "found " << clusters.components() << " clusters\n";

	stbi_image_free(pixels);
}

