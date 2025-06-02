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

void buildShapes(Clusters& clusters, size_t width, size_t height)
{
	auto index = [=] (size_t x, size_t y) { return x + y * width; };

	std::vector<vec2<float>> nodes;
	enum { ARITY = 4 };
	std::map<id_t, std::vector<std::array<size_t, ARITY>>> texel_edges;

	struct Offset {
		vec2<size_t> i;
		vec2<float> start;
		vec2<float> end;
	};
        
	const Offset directions[] = {
		{{ +1,  0 }, { +0.5f, -0.5f }, { +0.5f, +0.5f }},
		{{  0, +1 }, { -0.5f, +0.5f }, { +0.5f, +0.5f }},
        };

	for (size_t y = 0; y < height; ++y) {
                for (size_t x = 0; x < width; ++x) {
			vec2<size_t> current{ x, y };
                        auto vertex_current = index(x, y);

			for (const auto& dir : directions) {
				auto other = current + dir.i;
				if (other.x >= width || other.y >= height)
					continue;

				size_t vertex_other = index(other.x, other.y);
				if (clusters.repr(vertex_current) != clusters.repr(vertex_other)) {
					auto base = nodes.size();
					vec2<float> current_f{ static_cast<float>(current.x), static_cast<float>(current.y) };
					nodes.emplace_back(current_f + dir.start);
					nodes.emplace_back(current_f + dir.end);
					for (int i = 0; i < ARITY - 2; ++i) {
						nodes.push_back(lerp(nodes[base], nodes[base+1], static_cast<float>(i+1) / static_cast<float>(ARITY));
					}
					texel_edges[clusters.repr(vertex_current)].emplace_back(std::array{ base, base + 1, base + 2, base + 3 });
					texel_edges[clusters.repr(vertex_other  )].emplace_back(std::array{ base, base + 1, base + 2, base + 3 });
				}
			}
		}
	}

	std::vector<size_t> remap;
	for (size_t node = 0; node < nodes.size(); ++node) {
		remap.push_back(node);
	}
	for (size_t y = 0; y < height - 1; ++y) {
		for (size_t x = 0; x < width - 1; ++x) {
			auto bottom_right_R = 2 * ARITY * index(x, y) + 1;
			auto bottom_right_L = bottom_right_R + ARITY;
			auto bottom_left_L  = 2 * ARITY * index(x+1, y) + ARITY;
			auto top_right_R    = 2 * ARITY * index(x, y+1);
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

