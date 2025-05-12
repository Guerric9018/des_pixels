#include <vector>

using byte = unsigned char;

struct Color
{
	unsigned char r, g, b;
	Color() : r(0), g(0), b(0) {}
	Color(unsigned char r, unsigned char g, unsigned char b) : r(r), g(g), b(b) {}
};

struct Vertex
{
	int id;
	Color color;
};

struct Clusters
{
	using cluster = std::vector<Vertex>;
	std::vector<cluster> data;
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
