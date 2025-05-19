#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <iostream>
#include <cassert>
#include <vector>
#include "data.hpp"

std::vector<Shape> buildShapes(Clusters& clusters, size_t width, size_t height)
{
	auto index = [=] (size_t x, size_t y) { return x + y * width; };

	std::vector<Shape> shapes(clusters.components());

	struct Offset {
		int dx, dy;
		float fx, fy;
	};
        
	const Offset directions[4] = {
		{+1,  0, +0.5f,  0.0f},
		{-1, +1, -0.5f, +0.5f},
		{0, +1,  0.0f, +0.5f},
		{+1, +1, +0.5f, +0.5f},
        };

	for (size_t y = 0; y < height; ++y) {
                for (size_t x = 0; x < width; ++x) {
                        auto vertex_a = index(x, y);
			int cluster_a = clusters.repr(vertex_a);

			for (const auto& dir : directions) {
				int nx = static_cast<int>(x) + dir.dx;
				int ny = static_cast<int>(y) + dir.dy;
				
				if (nx < 0 || ny < 0 || nx >= static_cast<int>(width) || ny >= static_cast<int>(height))
					continue;

					
				size_t vertex_b = index(nx, ny);
				int cluster_b = clusters.repr(vertex_b);

				if (cluster_a != cluster_b) {
					vec2<float> nodes[4];
					nodes[0] = { static_cast<float>(x) + dir.fx,
						     static_cast<float>(y) + dir.fy };

					shapes[cluster_a].id = cluster_a;
					shapes[cluster_a].edges.push_back({cluster_a, nodes});

					shapes[cluster_b].id = cluster_b;
					shapes[cluster_b].edges.push_back({cluster_b, nodes});
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

