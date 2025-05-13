#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <iostream>
#include <cassert>
#include <vector>
#include "data.hpp"

#if 0
std::vector<Shape> buildShapes(Clusters& clusters, size_t width, size_t height)
{
	auto index = [=] (size_t x, size_t y) { return x + y * width; };

	std::vector<Shape> shapes(clusters.components());

        size_t offsets[] = {
                +1,
                -1 + width,
                +width,
                +1 + width,
        };

	float float_x_offsets[] = {
		+0.5f,
		-0.5f,
		+0.0f,
		+0.5f,
	};

	float float_y_offsets[] = {
		+0.0f,
		+0.5f,
		+0.5f,
		+0.5f,
	};
	
	for (size_t y = 0; y < height; ++y) {
                for (size_t x = 0; x < width; ++x) {
                        auto vertex_a = index(x, y);
			for (size_t direction = 0; direction < 4; ++direction) {
				auto vertex_b = vertex_a + offsets[direction];
					
				int cluster_a = clusters.find(vertex_a);
				int cluster_b = clusters.find(vertex_b);

				if (cluster_a != cluster_b) {
					vec2<float> nodes[4];
					nodes[0] = {(float)x + float_x_offsets[direction],
					      	    (float)y + float_y_offsets[direction]};

					Edge edge_a = {cluster_a, nodes};  
					Edge edge_b = {cluster_b, nodes};
					
					shapes[cluster_a].id = cluster_a;
					shapes[cluster_a].edges.push_back(edge_a);
					shapes[cluster_b].id = cluster_b;
					shapes[cluster_b].edges.push_back(edge_b);
				}
			}
		}
	}

	return shapes;
}
#endif

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

