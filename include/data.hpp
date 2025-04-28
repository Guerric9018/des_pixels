#include <vector>

struct Vertex
{
	int id;
};

struct Clusters
{
	std::vector<std::vector<Vertex>> cluster;
};

struct vec2
{
	union {
		struct { float x, y; };
		float v[2];
	};
};

struct Boundary
{
	std::vector<vec2> corner;
	std::vector<vec2> edge;
};
