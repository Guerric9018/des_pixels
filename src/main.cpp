#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <iostream>
#include <cassert>
#include <vector>
#include <map>
#include "data.hpp"

struct union_find
{
	std::vector<unsigned> data;

	union_find(size_t sz) : data(sz)
	{
		for (size_t i = 0; i < sz; ++i) {
			data[i] = i;
		}
	}

	unsigned find(unsigned id)
	{
		unsigned at = id;
		while (at != data[at]) {
			at = data[at];
		}
		return data[id] = at;
	}

	unsigned unite(unsigned a, unsigned b)
	{
		return data[find(b)] = find(a);
	}
};

bool same_color(byte *data, size_t a, size_t b)
{
	auto d0 = data[a * 3 + 0] - data[b * 3 + 0];
	auto d1 = data[a * 3 + 1] - data[b * 3 + 1];
	auto d2 = data[a * 3 + 2] - data[b * 3 + 2];
	return d0*d0 + d1*d1 + d2*d2 <= 8*8;
}

Color calculateRepresentativeColor(const Clusters::cluster& cluster)
{
	if (cluster.empty()) return Color();

	long totalR = 0, totalG = 0, totalB = 0;
	for (const auto& vertex : cluster) {
		totalR += vertex.color.r;
		totalG += vertex.color.g;
		totalB += vertex.color.b;
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

	auto clusters = find_clusters(pixels, width, height);
	std::cout << "found " << clusters.data.size() << " clusters\n";

	stbi_image_free(pixels);
}

