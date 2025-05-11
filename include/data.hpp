#include <vector>

using byte = unsigned char;

struct Vertex
{
	int id;
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
