#include <vector>
#include <map>
#include <cassert>

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
	std::vector<id_t> data;

	union_find(size_t sz) : data(sz, id_t(-1))
	{
	}

	id_t find(id_t id)
	{
		if (data[id] >= (1u << 31)) {
			return id;
		}
		id_t at = id;
		do {
			at = data[at];
		} while (data[at] < (1u << 31));
		return data[id] = at;
	}

	id_t unite(id_t a, id_t b)
	{
		a = find(a);
		b = find(b);
		if (a == b) {
			return a;
		}
		data[a] += data[b];
		return data[b] = a;
	}

	size_t count(id_t id)
	{
		return -data[find(id)];
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
private:
	size_t width;
	size_t height;

	using cluster = std::vector<Vertex>;
	std::map<id_t, cluster> cluster2vertex;
	union_find vertex2cluster;

public:
	Clusters(size_t width, size_t height, byte *data);
	id_t repr(size_t id);
	size_t components() const ;
	Color average_color(byte *data, id_t clust);

private:
	using conflict = std::pair<size_t, size_t>;
	std::vector<conflict> merge_nonconflicts(byte *data);
	void reverse_mapping();
	void conflict_resolution(std::vector<conflict> const& diagonals);
};

struct Edge
{
	int shape;
	vec2<float> nodes[4];
};

struct Shape
{
	int id;
	std::vector<Edge> edges;
};
