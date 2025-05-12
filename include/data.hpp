#include <vector>
#include <map>

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

struct Clusters
{
	using cluster = std::vector<Vertex>;
	std::map<id_t, cluster> cluster2vertex;
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

struct Boundary
{
	std::vector<vec2<float>> corner;
	std::vector<vec2<float>> edge;
};
