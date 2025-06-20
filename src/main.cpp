#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <iostream>
#include <cassert>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <cstdint>
#include "data.hpp"
#include "gfx.hpp"
#include "gfx/shader.hpp"
#include "gfx/buffer.hpp"
#include "gfx/render.hpp"

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
	std::vector<std::vector<size_t>> edge;
};

size_t bit(size_t b)
{
	return size_t{1} << b;
}

template <buffer_description D>
Mesh buildShapes(Clusters& clusters, size_t width, size_t height, Render &rdr, Render::draw_context<D> const &line_info)
{
	const auto index = [=] (size_t x, size_t y) { return y < height && x < width ? x + y * width: size_t(-1); };
	static const size_t ARITY = 3;

	size_t edge_node_end = 0;
	const size_t edge_node_max = 4*ARITY*width*height;
	std::vector<vec2<float>> nodes(edge_node_max);
	// edge between s and t and max(s,t) in edges[min(s,t)]
	std::map<size_t, std::vector<size_t>> edges;

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
		{ { 0, size_t(-1) }, { +1.0f,  0.0f } },
		{ { size_t(-1), 0 }, {  0.0f,  0.0f } },
		{ { 0, size_t(+1) }, {  0.0f, +1.0f } },
		{ { size_t(+1), 0 }, { +1.0f, +1.0f } },
	};

	for (size_t y = 0; y < height; ++y) {
		for (size_t x = 0; x < width; ++x) {
			const auto current = clusters.repr(index(x, y));
			for (size_t o = 0; o < std::size(offset); ++o) {
				const auto nx = x + offset[o].pixel.x;
				const auto ny = y + offset[o].pixel.y;
				const auto neighbr = clusters.repr(index(nx, ny));
				if (current == neighbr)
					continue;
				const auto pos = vec2<float>(float(x), float(y));
				const auto start = pos + offset[o].start;
				const auto end   = pos + offset[(o+1) % 4].start;
				for (size_t edge_node = 0; edge_node < ARITY; ++edge_node) {
					const float t = float(edge_node+1) / float(ARITY+1);
					if (edge_node < ARITY-1) {
						edges[edge_node_end].push_back(edge_node_end+1);
					}
					nodes[edge_node_end++] = lerp(start, end, t);
				}
				nodes.emplace_back(start);
				nodes.emplace_back(end  );
				edges[edge_node_end-ARITY].push_back(nodes.size()-2);
				edges[edge_node_end-1    ].push_back(nodes.size()-1);
				corners.push_back(choice{ x, y, o });
			}
		}
	}

	std::vector<vec4<float>> lines;
	for (const auto &[s, neighbrs] : edges) {
		for (const auto t : neighbrs) {
			const auto start = nodes[s];
			const auto end   = nodes[t];
			lines.push_back(vec4<float>(start.x, start.y, end.x, end.y));
		}
	}
	rdr.submit(line_info, lines, vec4<float>(0.8f, 0.9f, 0.8f, 1.0f), GL_LINES);
	rdr.draw();

	union_find node_map(nodes.size());
	static const float epsilon = std::pow(0.5f / float(ARITY + 1), 2);
	// static const float epsilon = 1e-6;
	// edge nodes
	std::vector<vec4<float>> links;
	for (size_t en1 = 0; en1 < edge_node_end; ++en1) {
		for (size_t en2 = en1 + 1; en2 < edge_node_end; ++en2) {
			if (dist2(nodes[en1], nodes[en2]) < epsilon) {
				node_map.unite(en1, en2);
				const auto start = nodes[en1];
				const auto end   = nodes[en2];
				links.push_back(vec4<float>{start.x, start.y, end.x, end.y});
			}
		}
	}

	rdr.keep();
	rdr.submit(line_info, links, vec4<float>(0.7f, 0.0f, 0.9f, 1.0f), GL_LINES);
	rdr.draw();

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
			const auto start = nodes[cn1];
			const auto end   = nodes[cn2];
			links.push_back(vec4<float>{start.x, start.y, end.x, end.y});
		}
	}

	rdr.submit(line_info, links, vec4<float>(0.1f, 0.8f, 0.9f, 1.0f), GL_LINES);
	rdr.draw();

	std::map<id_t, id_t> compress;
	for (id_t i = 0; i < node_map.data.size(); ++i) {
		compress.try_emplace(node_map.find(i), compress.size());
	}

	lines.clear();
	std::vector<vec2<float>> compressed_nodes(compress.size());
	std::map<size_t, std::set<size_t>> remapped_edges;
	for (const auto [orig, mapped] : compress) {
		compressed_nodes[mapped] = nodes[orig];
	}
	for (const auto [orig, mapped] : compress) {
		for (const auto neighbr : edges[orig]) {
			const auto mapped_neighbr = compress.find(node_map.find(neighbr));
			assert(mapped_neighbr != compress.end());
			const auto [min, max] = std::minmax(mapped, mapped_neighbr->second);
			assert(min != max);
			remapped_edges[min].emplace(max);
			const auto start = compressed_nodes[min];
			const auto end   = compressed_nodes[max];
			lines.push_back(vec4<float>(start.x, start.y, end.x, end.y));
		}
	}

	rdr.clear();
	rdr.submit(line_info, lines, vec4<float>(1.0f, 1.0f, 1.0f, 1.0f), GL_LINES);
	rdr.draw();

	lines.clear();
	std::vector<std::vector<size_t>> compressed_edges(remapped_edges.size());
	for (const auto &[s, neighbrs] : remapped_edges) {
		for (const auto t : neighbrs) {
			compressed_edges[s].push_back(t);
			const auto start = compressed_nodes[s];
			const auto end   = compressed_nodes[t];
			lines.push_back(vec4<float>(start.x, start.y, end.x, end.y));
		}
	}

	while (rdr.clear()) {
		rdr.submit(line_info, lines, vec4<float>(1.0f, 1.0f, 1.0f, 1.0f), GL_LINES);
		rdr.draw();
	}

	return Mesh{ std::move(compressed_nodes), std::move(compressed_edges) };
}

int main(int argc, char **argv)
{
	if (argc != 3) {
		std::cout << "Usage: " << argv[0] << " <source> <target>\n";
		return 1;
	}
	Window window("Depixel", 720, 720);

	Shader shader(
		ShaderStage(
			R"(#version 330 core
			layout(location = 0) in vec2 attr_pos;
			layout(location = 1) in vec2 inst_pos;
			uniform float scale;
			uniform float inner_scale;
			void main() {
				vec2 pos = (attr_pos * inner_scale + inst_pos) * scale;
				gl_Position = vec4(pos - vec2(0.5, 0.5), 0.0, 1.0);
			})", ShaderStage::Vertex),
		ShaderStage(
			R"(#version 330 core
			out vec4 frag_color;
			uniform vec4 color;
			void main() {
				frag_color = color;
			})", ShaderStage::Fragment)
	);
	shader.bind();

	struct attr0descr {
		using element_type = float;
		using attrib_type  = vec2<float>;
		using vertex_type  = vec2<float>;
		enum : GLint { element_count = 2 };
		enum : GLuint { location = 0 };
		enum : GLuint { instanced = 0 };
	};

	struct attr1descr {
		using element_type = float;
		using attrib_type  = vec2<float>;
		using vertex_type  = vec2<float>;
		enum : GLint { element_count = 2 };
		enum : GLuint { location = 1 };
		enum : GLuint { instanced = 1 };
	};

	struct attr2descr {
		using element_type = float;
		using attrib_type  = vec4<float>;
		using vertex_type  = vec4<float>;
		enum : GLint { element_count = 4 };
		enum : GLuint { location = 2 };
		enum : GLuint { instanced = 1 };
	};

	VertexArray va;
	{
		vec2<float> vertices[] = {
			{ 0.0f, 0.0f },
			{ 0.0f,+1.0f },
			{ 1.0f, 0.0f },
			{ 1.0f,+1.0f },
		};
		GLuint indices[] = {
			0, 1, 2,
			2, 1, 3,
		};
		VertexBuffer(vertices, attr0descr{}).leak();
		IndexBuffer(indices).leak();
	}
	size_t npos = 256;
	VertexBuffer vb_pos(std::span{static_cast<vec2<float>*>(nullptr), npos}, attr1descr{});
	Render::draw_context<attr1descr> info{shader, va, vb_pos, npos};

	int width, height, channels;
	stbi_set_flip_vertically_on_load(true);
	auto pixels = stbi_load(argv[1], &width, &height, &channels, 0);
	assert(channels == 3);
	assert(width > 0 && height > 0);

	Clusters clusters(width, height, pixels);
	std::cout << "found " << clusters.components() << " clusters\n";

	vec4<float> colors[] = {
		{ 0.8f, 0.1f, 0.2f, 1.0f },
		{ 0.6f, 0.7f, 0.1f, 1.0f },
		{ 0.2f, 0.9f, 0.4f, 1.0f },
		{ 0.1f, 0.5f, 0.7f, 1.0f },
		{ 0.2f, 0.1f, 0.8f, 1.0f },
	};
	std::vector<std::vector<vec2<float>>> cluster_pos;
	for (const auto &[_, cluster] : clusters.get()) {
		cluster_pos.emplace_back();
		for (const auto &[xy] : cluster) {
			const auto x = xy % width;
			const auto y = xy / width;
			cluster_pos.back().emplace_back(float(x), float(y));
		}
	}

	shader.set("inner_scale", 1.0f);
	shader.set("scale", 1.2f / width);

	Shader line_shader(
		ShaderStage(R"(#version 330 core
			layout(location = 2) in vec4 inst_endpoints;
			uniform float scale;
			void main() {
				vec2 pos = inst_endpoints.xy;
				if ((gl_VertexID & 1) == 1) {
					pos = inst_endpoints.zw;
				}
				gl_Position = vec4(pos * scale - vec2(0.5, 0.5), 0.0, 1.0);
			})", ShaderStage::Vertex),
		ShaderStage(R"(#version 330 core
			out vec4 frag_color;
			uniform vec4 color;
			void main() {
				frag_color = color;
			})", ShaderStage::Fragment)
	);
	line_shader.bind();
	line_shader.set("scale", 1.2f / width);

	VertexArray line_va;
	{
		GLuint indices[] = { 0, 1 };
		IndexBuffer(indices).leak();
	}
	size_t nline = 256;
	VertexBuffer line_vb(std::span{static_cast<vec4<float>*>(nullptr), nline}, attr2descr{});
	Render::draw_context<attr2descr> line_info{line_shader, line_va, line_vb, nline};

	Render rdr(std::move(window));
	for (size_t ic = 0; ic < clusters.components(); ++ic) {
		rdr.submit(info, cluster_pos[ic], colors[ic % std::size(colors)], GL_QUADS);
	}
	rdr.keep();

	auto mesh = buildShapes(clusters, width, height, rdr, line_info);

	stbi_image_free(pixels);
}

