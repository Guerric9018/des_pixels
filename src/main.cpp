#include <algorithm>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <iostream>
#include <cassert>
#include <vector>
#include <map>
#include "data.hpp"

Color calculateRepresentativeColor(byte *data, const Clusters::cluster& cluster)
{
	if (cluster.empty()) return Color();

	long totalR = 0, totalG = 0, totalB = 0;
	for (const auto& vertex : cluster) {
		auto color = vertex.color(data);
		totalR += color.r;
		totalG += color.g;
		totalB += color.b;
	}

	return Color(
		static_cast<unsigned char>(totalR / cluster.size()),
		static_cast<unsigned char>(totalG / cluster.size()),
		static_cast<unsigned char>(totalB / cluster.size())
	);
}

Clusters find_clusters(byte *data, size_t width, size_t height)
{
	Clusters c;

	auto index = [=] (size_t x, size_t y) { return x + y * width; };
	size_t offsets[] = {
		+1,
		-1 + width,
		+width,
		+1 + width,
	};
	union_find uf(width * height);
	for (size_t y = 0; y < height; ++y) {
		for (size_t x = 0; x < width; ++x) {
			auto base = index(x, y);
			for (auto offset : offsets) {
				auto at = base + offset;
				if (at >= uf.data.size()) {
					continue;
				}
				if (same_color(data, base, at)) {
					uf.unite(base, at);
				}
			}
		}
	}

	std::map<unsigned, std::vector<size_t>> mapping;
	for (size_t y = 0; y < height; ++y) {
		for (size_t x = 0; x < width; ++x) {
			auto i = index(x, y);
			mapping[uf.find(i)].push_back(i);
		}
	}

	c.data = std::vector<Clusters::cluster>(mapping.size());

	return c;
}

std::vector<Shape> buildShapes(Clusters& clusters, int width, int height)
{
	auto index = [=] (size_t x, size_t y) { return x + y * width; };

	std::vector<Shape> shapes(clusters.data.size());

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
	}

	float float_y_offsets[] = {
		+0.0f,
		+0.5f,
		+0.5f,
		+0.5f,
	}
	
	for (size_t y = 0; y < height; ++y) {
                for (size_t x = 0; x < width; ++x) {
                        auto vertex_a = index(x, y);
			for (size_t direction = 0; direction < 4; ++direction) {
				auto vertex_b = vertex_a + offsets[direction];
					
				int cluster_a = clusters.find(vertex_a);
				int cluster_b = clusters.find(vertex_b);

				if (cluster_a != cluster_b) {
					vec2 nodes[4];
					nodes[0] = {(float)x + float_x_offsets[direction],
					      	    (float)y + float_y_offsets[direction]}	

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
	std::cout << "found " << clusters.cluster2vertex.size() << " clusters\n";

	stbi_image_free(pixels);
}

