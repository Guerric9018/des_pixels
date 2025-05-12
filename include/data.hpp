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

struct vec2
{
	float x;
	float y;

	float &operator[](size_t i) { return (&x)[i]; }
	const float &operator[](size_t i) const { return (&x)[i]; }
};

struct Boundary
{
	std::vector<vec2> corner;
	std::vector<vec2> edge;
};
