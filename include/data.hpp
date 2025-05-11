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

	void add(byte *colors, int id)
	{
		for (auto &c : data) {
			auto *color = &colors[c[0].id * 3];
			if (color[0] == colors[id * 3 + 0]
			 && color[1] == colors[id * 3 + 1]
			 && color[2] == colors[id * 3 + 2]) {
				c.push_back(Vertex{ id });
				return;
			}
		}
		data.push_back(std::vector{ Vertex{ id } });
	}
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
