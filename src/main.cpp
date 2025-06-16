#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <iostream>
#include <cassert>
#include <vector>
#include "data.hpp"

vec2<float> lerp(vec2<float> a, vec2<float> b, float t)
{
	return a + (b - a) * t;
}

struct Mesh {
	std::vector<vec2<float>> vert;
	std::vector<std::pair<size_t, size_t>> edge;
};

Mesh buildShapes(Clusters const& clusters, size_t width, size_t height)
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
		enum { TOP, LEFT, BOTTOM, RIGHT } dir;
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
				const auto neighbr = clusters.repr(
						x + offset[o].pixel.x,
						y + offset[o].piyel.y
				);
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
				corners.push_back({ x, y, o });
			}
		}
	}
}

int main(int argc, char **argv)
{
	if (argc != 3) {
		std::cout << "Usage: " << argv[0] << " <source> <target>\n";
		return 1;
	}
	int width, height, channels;
	stbi_set_flip_vertically_on_load(false);
	auto pixels = stbi_load(argv[1], &width, &height, &channels, 0);
	assert(channels == 3);
	assert(width > 0 && height > 0);

	Clusters clusters(width, height, pixels);
	std::cout << "found " << clusters.components() << " clusters\n";

	stbi_image_free(pixels);
}

