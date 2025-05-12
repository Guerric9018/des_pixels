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

