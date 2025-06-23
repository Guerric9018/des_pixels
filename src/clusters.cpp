#include "data.hpp"
#include <cassert>
#include <vector>
#include <iostream>

Clusters::Clusters(size_t width, size_t height, byte *data, int delta_c)
	: width(width), height(height), vertex2cluster(width * height)
{
	auto diag = merge_nonconflicts(data, delta_c);
	conflict_resolution(diag);
	reverse_mapping();
	
	// find all cluster colors
	for (const auto &[clust, _] : cluster2vertex) {
		avg[clust] = average_color(data, clust);
	}
}

id_t Clusters::repr(size_t id)
{
	if (id >= vertex2cluster.data.size())
		return id_t(-1);
	return vertex2cluster.find(id);
}

std::vector<Clusters::conflict> Clusters::merge_nonconflicts(byte *data, int delta_c)
{
	std::vector<conflict> diagonals;
	auto index = [=] (size_t x, size_t y) { return x + y * width; };
	vec2<size_t> offsets[] = {
		{ +1,  0 },
		{  0, +1 },
		{ size_t(-1), +1 }, // overflow is fine
		{ +1, +1 },
	};
	auto same_color = [delta_c] (byte *d, size_t a, size_t b) {
		auto d0 = d[a * 3 + 0] - d[b * 3 + 0];
		auto d1 = d[a * 3 + 1] - d[b * 3 + 1];
		auto d2 = d[a * 3 + 2] - d[b * 3 + 2];
		return d0*d0 + d1*d1 + d2*d2 <= delta_c*delta_c;
	};
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
					vertex2cluster.unite(base, at);
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
					if (same_color(data, at - width, base + width)) {
						// avoid the counter diagonal to add duplicate diagonals
						if (ioffset != 3) {
							diagonals.push_back(std::make_pair(base, at));
						}
					} else {
						vertex2cluster.unite(base, at);
					}
				}
			}
		}
	}
	return diagonals;
}

void Clusters::reverse_mapping()
{
	for (id_t i = 0; i < height * width; ++i) {
		cluster2vertex[vertex2cluster.find(i)].push_back(Vertex{ i });
	}
}	

void Clusters::conflict_resolution(std::vector<conflict> const& diagonals)
{
	for (auto [base, at] : diagonals) {
		id_t main_diag_a = vertex2cluster.find(base);
		id_t main_diag_b = vertex2cluster.find(at);
		id_t counter_diag_a = vertex2cluster.find(at - width);
		id_t counter_diag_b = vertex2cluster.find(base + width);
		size_t merge_a, merge_b;
		if (std::min(vertex2cluster.count(main_diag_a), vertex2cluster.count(main_diag_b))
		  < std::min(vertex2cluster.count(counter_diag_a), vertex2cluster.count(counter_diag_b))) {
			vertex2cluster.unite(main_diag_a, main_diag_b);
		} else {
			vertex2cluster.unite(counter_diag_a, counter_diag_b);
		}
	}
}

size_t Clusters::components() const
{
	return cluster2vertex.size();
}

Color Clusters::average_color(byte *data, id_t clust)
{
	auto &cluster = cluster2vertex[clust];
	if (cluster.empty()) return Color();

	long totalR = 0, totalG = 0, totalB = 0;
	for (const auto& vertex : cluster) {
		auto color = vertex.color(data);
		totalR += color.r;
		totalG += color.g;
		totalB += color.b;
	}

	const auto col = Color(
		static_cast<unsigned char>(totalR / cluster.size()),
		static_cast<unsigned char>(totalG / cluster.size()),
		static_cast<unsigned char>(totalB / cluster.size())
	);
	return col;
}

Color Clusters::average_color(id_t clust) const
{
	const auto pos = avg.find(clust);
	assert(pos != avg.end());
	return pos->second;
}

std::map<id_t, Clusters::cluster> const &Clusters::get() const
{
	return cluster2vertex;
}
