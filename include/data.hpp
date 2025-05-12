#include <vector>
#include <map>

using byte = unsigned char;
using id_t = unsigned;

struct Vertex
{
	id_t id;
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
