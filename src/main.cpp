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
	std::vector<std::array<size_t, 4>> texel_edges;

	struct Offset {
		vec2<size_t> i;
		vec2<float> start;
		vec2<float> end;
	};
        
	const Offset directions[4] = {
		{{ size_t(-1),  0 }, { -0.5f, -0.5f }, { -0.5f, +0.5f }},
		{{  0, +1 }, { -0.5f, +0.5f }, { +0.5f, +0.5f }},
		{{ +1,  0 }, { +0.5f, -0.5f }, { +0.5f, +0.5f }},
		{{  0, size_t(-1) }, { -0.5f, -0.5f }, { +0.5f, -0.5f }},
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
					nodes.push_back(lerp(nodes[base], nodes[base+1], 1.0f / 3.0f));
					nodes.push_back(lerp(nodes[base], nodes[base+1], 2.0f / 3.0f));
					texel_edges.emplace_back(std::array{ base, base + 1, base + 2, base + 3 });
				}
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

