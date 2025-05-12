#include <vector>
#include <map>
#include <algorithm>

using byte = unsigned char;
using id_t = unsigned;

struct Color
{
	unsigned char r, g, b;

	Color(unsigned char r = 0, unsigned char g = 0, unsigned char b = 0)
		: r(r), g(g), b(b)
	{
	}
};

struct Vertex
{
	id_t id;

	Color color(byte *data) const
	{
		return Color(data[id * 3 + 0], data[id * 3 + 1], data[id * 3 + 2]);
	}
};

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

template <typename D>
struct vec2
{
	D x;
	D y;

	D &operator[](size_t i) { return (&x)[i]; }
	const D &operator[](size_t i) const { return (&x)[i]; }
};

template <typename D>
inline vec2<D> operator+(vec2<D> lhs, vec2<D> rhs)
{
	return vec2{ lhs.x + rhs.x, lhs.y + rhs.y };
}

template <typename D>
inline vec2<D> operator-(vec2<D> lhs, vec2<D> rhs)
{
	return vec2{ lhs.x - rhs.x, lhs.y - rhs.y };
}

template <typename D>
inline vec2<D> operator/(vec2<D> lhs, D div)
{
	return vec2{ lhs.x / div, lhs.y / div };
}

struct Clusters
{
	size_t width;
	size_t height;

	using cluster = std::vector<Vertex>;
	std::map<id_t, cluster> cluster2vertex;
	union_find vertex2cluster;

	Clusters(size_t width, size_t height, byte *data)
		: width(width), height(height), vertex2cluster(width * height)
	{
		auto diag = merge_nonconflicts(data);
		partial_mapping();
		conflict_resolution(diag);
	}

	id_t repr(size_t id)
	{
		return vertex2cluster.find(id);
	}

private:
	using conflict = std::pair<size_t, size_t>;
	std::vector<conflict> merge_nonconflicts(byte *data)
	{
		std::vector<conflict> diagonals;
		auto index = [=] (size_t x, size_t y) { return x + y * width; };
		vec2<size_t> offsets[] = {
			{ +1,  0 },
			{  0, +1 },
			{ size_t(-1), +1 }, // overflow is fine
			{ +1, +1 },
		};
		auto same_color = [] (byte *d, size_t a, size_t b) {
			auto d0 = d[a * 3 + 0] - d[b * 3 + 0];
			auto d1 = d[a * 3 + 1] - d[b * 3 + 1];
			auto d2 = d[a * 3 + 2] - d[b * 3 + 2];
			return d0*d0 + d1*d1 + d2*d2 <= 8*8;
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
						// avoid the counter diagonal to add duplicate diagonals
						if (same_color(data, at - width, base + width) && ioffset != 3) {
							diagonals.push_back(std::make_pair(base, at));
						} else {
							vertex2cluster.unite(base, at);
						}
					}
				}
			}
		}
		return diagonals;
	}

	void partial_mapping()
	{
		for (id_t i = 0; i < height * width; ++i) {
			cluster2vertex[vertex2cluster.find(i)].push_back(Vertex{ i });
		}
	}

	void conflict_resolution(std::vector<conflict> const& diagonals)
	{
		for (auto [base, at] : diagonals) {
			struct cluster_cardinality { size_t cardinality; size_t id; };
			cluster_cardinality choices[4];
			choices[0] = { cluster2vertex[vertex2cluster.find(base)].size(), base };
			choices[1] = { cluster2vertex[vertex2cluster.find(at)].size(), at };
			choices[2] = { cluster2vertex[vertex2cluster.find(at - width)].size(), at - width };
			choices[3] = { cluster2vertex[vertex2cluster.find(base + width)].size(), base + width };
			auto best = std::min_element(std::begin(choices), std::end(choices), [] (auto c1, auto c2) { return c1.cardinality - c2.cardinality; });
			size_t merge_a, merge_b;
			if (best->id == base || best->id == at) {
				merge_a = vertex2cluster.find(base);
				merge_b = vertex2cluster.find(at);
			} else {
				merge_a = vertex2cluster.find(at - width);
				merge_b = vertex2cluster.find(base + width);
			}
			auto merge_target = vertex2cluster.unite(merge_a, merge_b);
			auto merge_source = merge_target ^ merge_a ^ merge_b;
			auto merge_data = cluster2vertex.extract(merge_source).mapped();
			auto &merged = cluster2vertex[merge_target];
			for (auto elt : merge_data) {
				merged.push_back(elt);
			}
		}
	}
};

struct Boundary
{
	std::vector<vec2<float>> corner;
	std::vector<vec2<float>> edge;
};
