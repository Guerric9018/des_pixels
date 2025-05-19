#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <iostream>
#include <cassert>
#include <vector>
#include "data.hpp"

std::vector<Shape> buildShapes(Clusters& clusters, size_t width, size_t height)
{
	auto index = [=] (size_t x, size_t y) { return x + y * width; };

	std::vector<vec2<float>> nodes;
	std::vector<std::array<size_t, 4>> texel_edges;

	struct Offset {
		int dx, dy;
		float fx_start, fy_start;
		float fx_end, fy_end;
	};
        
	const Offset directions[4] = {
		{-1,  0, -0.5f, -0.5f, -0.5f, +0.5f},
		{ 0, +1, -0.5f, +0.5f, +0.5f, +0.5f},
		{+1,  0, +0.5f, -0.5f, +0.5f, +0.5f},
		{ 0, -1, -0.5f, -0.5f, +0.5f, -0.5f},
        };

	for (size_t y = 0; y < height; ++y) {
                for (size_t x = 0; x < width; ++x) {
                        auto vertex_current = index(x, y);
			int cluster_current = clusters.repr(vertex_current);

			for (const auto& dir : directions) {
				int nx = static_cast<int>(x) + dir.dx;
				int ny = static_cast<int>(y) + dir.dy;
				
				if (nx < 0 || ny < 0 || nx >= static_cast<int>(width) || ny >= static_cast<int>(height))
					continue;

					
				size_t vertex_other = index(nx, ny);
				int cluster_other = clusters.repr(vertex_other);

				if (cluster_current != cluster_other) {
					auto base = nodes.size();
					
					nodes.push_back({ static_cast<float>(x) + dir.fx_start,
						          static_cast<float>(y) + dir.fy_start });
					nodes.push_back({ static_cast<float>(x) + dir.fx_end,
						          static_cast<float>(y) + dir.fy_end });
					nodes.push_back({ edge[0] + 1.0f * (edge[1] - edge[0]) / 3.0f });
					nodes.push_back({ edge[0] + 2.0f * (edge[1] - edge[0]) / 3.0f });

					texel_edges.emplace_back(base, base + 1, base + 2, base + 3);
				}
			}
		}
	}

	return shapes;
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

