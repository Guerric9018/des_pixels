#include <algorithm>
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
	auto index = [=] (size_t x, size_t y) { return x + y * width; };
	vec2<size_t> offsets[] = {
		{ +1,  0 },
		{  0, +1 },
		{ size_t(-1), +1 }, // overflow is fine
		{ +1, +1 },
	};
	union_find uf(width * height);
	std::vector<std::pair<size_t, size_t>> diagonals;
	for (size_t y = 0; y < height; ++y) {
		for (size_t x = 0; x < width; ++x) {
			auto base = index(x, y);
			// axis-aligned offsets
			for (size_t ioffset = 0; ioffset < 2; ++ioffset) {
				vec2<size_t> at_pos{ x + offsets[ioffset].x, y + offsets[ioffset].y };
				if (at_pos.x >= width || at_pos.y >= height) {
					continue;
				}
				auto at = index(at_pos.x, at_pos.y);
				if (same_color(data, base, at)) {
					uf.unite(base, at);
				}
			}
			// diagonal offsets
			for (size_t ioffset = 2; ioffset < 4; ++ioffset) {
				vec2<size_t> at_pos{ x + offsets[ioffset].x, y + offsets[ioffset].y };
				if (at_pos.x >= width || at_pos.y >= height) {
					continue;
				}
				auto at = index(at_pos.x, at_pos.y);
				// when this passes, we know the corresponding pixels adjacent have indices in bounds
				if (same_color(data, base, at)) {
					// avoid the counter diagonal to add duplicate diagonals
					if (same_color(data, at - width, base + width) && ioffset != 3) {
						diagonals.push_back(std::make_pair(base, at));
					} else {
						uf.unite(base, at);
					}
				}
			}
		}
	}

	Clusters c;
	for (size_t y = 0; y < height; ++y) {
		for (size_t x = 0; x < width; ++x) {
			id_t i = index(x, y);
			c.cluster2vertex[uf.find(i)].push_back(Vertex{ i });
		}
	}

	for (auto [base, at] : diagonals) {
		struct cluster_cardinality { size_t cardinality; size_t id; };
		cluster_cardinality choices[4];
		choices[0] = { c.cluster2vertex[uf.find(base)].size(), base };
		choices[1] = { c.cluster2vertex[uf.find(at)].size(), at };
		choices[2] = { c.cluster2vertex[uf.find(at - width)].size(), at - width };
		choices[3] = { c.cluster2vertex[uf.find(base + width)].size(), base + width };
		auto best = std::min_element(std::begin(choices), std::end(choices), [] (auto c1, auto c2) { return c1.cardinality - c2.cardinality; });
		size_t merge_a, merge_b;
		if (best->id == base || best->id == at) {
			merge_a = uf.find(base);
			merge_b = uf.find(at);
		} else {
			merge_a = uf.find(at - width);
			merge_b = uf.find(base + width);
		}
		auto merge_target = uf.unite(merge_a, merge_b);
		auto merge_source = merge_target ^ merge_a ^ merge_b;
		auto merge_data = c.cluster2vertex.extract(merge_source).mapped();
		auto &merged = c.cluster2vertex[merge_target];
		for (auto elt : merge_data) {
			merged.push_back(elt);
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
	stbi_set_flip_vertically_on_load(false);
	auto pixels = stbi_load(argv[1], &width, &height, &channels, 0);
	assert(channels == 3);
	assert(width > 0 && height > 0);

	auto clusters = find_clusters(pixels, width, height);
	std::cout << "found " << clusters.cluster2vertex.size() << " clusters\n";

	stbi_image_free(pixels);
}

