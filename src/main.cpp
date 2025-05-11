#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <iostream>
#include <cassert>
#include <vector>
#include "data.hpp"

Clusters find_clusters(byte *data, size_t width, size_t height)
{
	Clusters c;
	for (size_t x = 0; x < width; ++x) {
		for (size_t y = 0; y < height; ++y) {
			auto id = y * height + x;
			c.add(data, id);
		}
	}
	return c;
}

int main(int argc, char **argv)
{
	if (argc != 3) {
		std::cout << "Usage: " << argv[0] << " <source> <target>\n";
		return 1;
	}
	int width, height, channels;
	auto pixels = stbi_load(argv[1], &width, &height, &channels, 0);
	assert(channels == 3);
	assert(width > 0 && height > 0);

	auto clusters = find_clusters(pixels, width, height);
	std::cout << "found " << clusters.data.size() << " clusters\n";

	stbi_image_free(pixels);
}

