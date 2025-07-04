#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <iostream>
#include <format>
#include <cassert>
#include <vector>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <sstream>
#include <fstream>
#include <string_view>
#include <charconv>
#include <algorithm>
#include <deque>
#include <cstdint>
#include <thread>
#include <span>
#include <string>
#include "data.hpp"
#include "gfx.hpp"
#include "gfx/shader.hpp"
#include "gfx/buffer.hpp"
#include "gfx/render.hpp"
#include "imgui.h"
#include "imconfig.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

using MaskT = uint64_t; 

static int clicks = 0;

std::string readFile(std::string_view path)
{
	std::ifstream file(path.data());
	file.seekg(0, std::ios_base::end);
	const auto len = file.tellg();
	std::string result(len + 1l, '\0');
	file.seekg(0, std::ios_base::beg);
	file.read(result.data(), len);
	return result;
}

struct attr0descr {
	using element_type = float;
	using attrib_type  = vec2<float>;
	using vertex_type  = vec2<float>;
	enum : GLint { element_count = 2 };
	enum : GLuint { location = 0 };
	enum : GLuint { instanced = 0 };
};

struct attr1descr {
	using element_type = float;
	using attrib_type  = vec2<float>;
	using vertex_type  = vec2<float>;
	enum : GLint { element_count = 2 };
	enum : GLuint { location = 1 };
	enum : GLuint { instanced = 1 };
};

struct attr2descr {
	using element_type = float;
	using attrib_type  = vec4<float>;
	using vertex_type  = vec4<float>;
	enum : GLint { element_count = 4 };
	enum : GLuint { location = 2 };
	enum : GLuint { instanced = 1 };
};

vec2<float> lerp(vec2<float> a, vec2<float> b, float t)
{
	return a + (b - a) * t;
}

float dist2(vec2<float> a, vec2<float> b)
{
	const auto delta = b - a;
	return delta.x * delta.x + delta.y * delta.y;
}

struct Mesh {
	struct EdgeEnd {
		size_t endpoint;
		id_t clust1;
		id_t clust2;

	};

	std::vector<vec2<float>> vert;
	std::vector<std::vector<EdgeEnd>> edge;
	std::vector<MaskT> node_cluster_ids;
};

static auto operator<=>(Mesh::EdgeEnd const &l, Mesh::EdgeEnd const &r)
{
	return l.endpoint <=> r.endpoint;
}

size_t bit(size_t b)
{
	return size_t{1} << b;
}

template <buffer_description D>
Mesh buildShapes(Clusters& clusters, size_t width, size_t height, Render &rdr, Render::draw_context<D> const &line_info)
{
	const auto index = [=] (size_t x, size_t y) { return y < height && x < width ? x + y * width: size_t(-1); };
	static const size_t ARITY = 3;

	size_t edge_node_end = 0;
	const size_t edge_node_max = 4*ARITY*width*height;
	std::vector<vec2<float>> nodes(edge_node_max);
	std::vector<MaskT> node_cluster_ids(edge_node_max, 0); 
	// edge between s and t and max(s,t) in edges[min(s,t)]
	std::map<size_t, std::vector<Mesh::EdgeEnd>> edges;

	std::map<id_t, size_t> cluster_to_compact;
	size_t cluster_compact_id = 0;
	for (const auto &[cluster_id, _] : clusters.get()) {
		cluster_to_compact[cluster_id] = cluster_compact_id++;
	}

	if (cluster_compact_id > 63) {
		std::cerr << "Error. Too many colors" << std::endl;
		std::exit(EXIT_FAILURE);
	}

	struct choice {
		size_t x;
		size_t y;
		size_t dir;
	};
	std::vector<choice> corners;

	struct offset_t {
		vec2<size_t> pixel;
		vec2<float > start;
	};

	offset_t offset[] = {
		{ { 0, size_t(-1) }, { +1.0f,  0.0f } },
		{ { size_t(-1), 0 }, {  0.0f,  0.0f } },
		{ { 0, size_t(+1) }, {  0.0f, +1.0f } },
		{ { size_t(+1), 0 }, { +1.0f, +1.0f } },
	};

	for (size_t y = 0; y < height; ++y) {
		for (size_t x = 0; x < width; ++x) {
			const auto current = clusters.repr(index(x, y));
			for (size_t o = 0; o < std::size(offset); ++o) {
				const auto nx = x + offset[o].pixel.x;
				const auto ny = y + offset[o].pixel.y;
				const auto neighbr = clusters.repr(index(nx, ny));
				if (current == neighbr)
					continue;
				const auto pos = vec2<float>(float(x), float(y));
				const auto start = pos + offset[o].start;
				const auto end   = pos + offset[(o+1) % 4].start;
				for (size_t edge_node = 0; edge_node < ARITY; ++edge_node) {
					const float t = float(edge_node+1) / float(ARITY+1);
					if (edge_node < ARITY-1) {
						edges[edge_node_end].emplace_back(edge_node_end+1, current, neighbr);
					}
					node_cluster_ids[edge_node_end] |= (1ULL << cluster_to_compact[current]);
					nodes[edge_node_end++] = lerp(start, end, t);
				}
				nodes.emplace_back(start);
				nodes.emplace_back(end  );
				node_cluster_ids.emplace_back(1ULL << cluster_to_compact[current]);
				node_cluster_ids.emplace_back(1ULL << cluster_to_compact[current]);
				edges[edge_node_end-ARITY].emplace_back(nodes.size()-2, current, neighbr);
				edges[edge_node_end-1    ].emplace_back(nodes.size()-1, current, neighbr);
				corners.push_back(choice{ x, y, o });
			}
		}
	}

	std::vector<vec4<float>> lines;
	for (const auto &[s, neighbrs] : edges) {
		for (const auto [t, c1, c2] : neighbrs) {
			const auto start = nodes[s];
			const auto end   = nodes[t];
			lines.push_back(vec4<float>(start.x, start.y, end.x, end.y));
		}
	}
	rdr.submit(line_info, lines, vec4<float>(0.8f, 0.9f, 0.8f, 1.0f), GL_LINES);
	rdr.draw();

	union_find node_map(nodes.size());
	static const float epsilon = std::pow(0.5f / float(ARITY + 1), 2);
	// static const float epsilon = 1e-6;
	// edge nodes
	std::vector<vec4<float>> links;
	for (size_t en1 = 0; en1 < edge_node_end; ++en1) {
		for (size_t en2 = en1 + 1; en2 < edge_node_end; ++en2) {
			if (dist2(nodes[en1], nodes[en2]) < epsilon) {
				size_t root1 = node_map.find(en1);
                size_t root2 = node_map.find(en2);
				if (root1 != root2) {
					node_map.unite(en1, en2);
					size_t new_root = node_map.find(root1);

					node_cluster_ids[new_root] |= node_cluster_ids[root1] | node_cluster_ids[root2];

					const auto start = nodes[en1];
					const auto end   = nodes[en2];
					links.push_back(vec4<float>{start.x, start.y, end.x, end.y});
				}
			}
		}
	}

	rdr.keep();
	rdr.submit(line_info, links, vec4<float>(0.7f, 0.0f, 0.9f, 1.0f), GL_LINES);
	rdr.draw();

	assert(nodes.size() - edge_node_max == 2*corners.size());
	// corner nodes
	for (size_t cn1 = edge_node_max; cn1 < nodes.size(); ++cn1) {
		for (size_t cn2 = cn1 + 1; cn2 < nodes.size(); ++cn2) {
			if (dist2(nodes[cn1], nodes[cn2]) >= epsilon)
				continue;
			const auto is_end_1   = (cn1 - edge_node_max) % 2;
			const auto [x, y, o1] = corners[(cn1 - edge_node_max) / 2];
			const auto is_end_2   = (cn2 - edge_node_max) % 2;
			const auto o2         = corners[(cn2 - edge_node_max) / 2].dir;
			id_t main1, main2, cntr1, cntr2;
			switch ((is_end_1 + o1) % 4) {
			case 0:
				main1 = clusters.repr(index(x  ,y-1));
				main2 = clusters.repr(index(x+1,y  ));
				cntr1 = clusters.repr(index(x+1,y-1));
				cntr2 = clusters.repr(index(x  ,y  ));
				break;
			case 1:
				main1 = clusters.repr(index(x-1,y-1));
				main2 = clusters.repr(index(x  ,y  ));
				cntr1 = clusters.repr(index(x  ,y-1));
				cntr2 = clusters.repr(index(x-1,y  ));
				break;
			case 2:
				main1 = clusters.repr(index(x-1,y  ));
				main2 = clusters.repr(index(x  ,y+1));
				cntr1 = clusters.repr(index(x  ,y  ));
				cntr2 = clusters.repr(index(x-1,y+1));
				break;
			case 3:
				main1 = clusters.repr(index(x  ,y  ));
				main2 = clusters.repr(index(x+1,y+1));
				cntr1 = clusters.repr(index(x+1,y  ));
				cntr2 = clusters.repr(index(x  ,y+1));
				break;
			}

			if (main1 == main2 && main1 != cntr1 && main1 != cntr2) {
				static const auto side = 0b01011010;
				const auto side1 = side >> (o1<<1|is_end_1);
				const auto side2 = side >> (o2<<1|is_end_2);
				if ((side1 & 1) != (side2 & 1))
					continue;
			} else if (cntr1 == cntr2 && cntr1 != main1 && cntr1 != main2) {
				static const auto side = 0b10010110;
				const auto side1 = side >> (o1<<1|is_end_1);
				const auto side2 = side >> (o2<<1|is_end_2);
				if ((side1 & 1) != (side2 & 1))
					continue;
			}

			// merge
			size_t root1 = node_map.find(cn1);
			size_t root2 = node_map.find(cn2);
			if (root1 != root2) {
				node_map.unite(cn1, cn2);
				size_t new_root = node_map.find(root1);

				node_cluster_ids[new_root] |= node_cluster_ids[root1] | node_cluster_ids[root2];
				
				const auto start = nodes[cn1];
				const auto end   = nodes[cn2];
				links.push_back(vec4<float>{start.x, start.y, end.x, end.y});
			}
		}
	}

	rdr.submit(line_info, links, vec4<float>(0.1f, 0.8f, 0.9f, 1.0f), GL_LINES);
	rdr.draw();

	std::map<id_t, id_t> compress;
	for (id_t i = 0; i < edge_node_end; ++i) {
		compress.try_emplace(node_map.find(i), compress.size());
	}
	for (id_t i = edge_node_max; i < nodes.size(); ++i) {
		compress.try_emplace(node_map.find(i), compress.size());
	}

	lines.clear();
	std::vector<vec2<float>> compressed_nodes(compress.size());
	std::vector<MaskT> compressed_node_cluster_ids(compress.size());
	std::map<size_t, std::set<Mesh::EdgeEnd>> remapped_edges;
	for (const auto [orig, mapped] : compress) {
		compressed_nodes[mapped] = nodes[orig];
		compressed_node_cluster_ids[mapped] = node_cluster_ids[orig];
	}
	for (size_t orig = 0; orig < nodes.size(); ++orig) {
		if (orig >= edge_node_end && orig < edge_node_max)
			continue;
		const auto mapped = compress.find(node_map.find(orig));
		assert(mapped != compress.end());
		for (const auto [neighbr, c1, c2] : edges[orig]) {
			const auto mapped_neighbr = compress.find(node_map.find(neighbr));
			assert(mapped_neighbr != compress.end());
			const auto [min, max] = std::minmax(mapped->second, mapped_neighbr->second);
			assert(min != max);
			remapped_edges[min].emplace(max, c1, c2);
			const auto start = compressed_nodes[min];
			const auto end   = compressed_nodes[max];
			lines.push_back(vec4<float>(start.x, start.y, end.x, end.y));
		}
	}

	rdr.clear();
	rdr.submit(line_info, lines, vec4<float>(1.0f, 1.0f, 1.0f, 1.0f), GL_LINES);
	rdr.draw();

	lines.clear();
	std::vector<std::vector<Mesh::EdgeEnd>> compressed_edges(compressed_nodes.size());
	for (const auto &[s, neighbrs] : remapped_edges) {
		for (const auto [t, c1, c2] : neighbrs) {
			compressed_edges[s].emplace_back(t, c1, c2);
			compressed_edges[t].emplace_back(s, c1, c2);
			const auto start = compressed_nodes[s];
			const auto end   = compressed_nodes[t];
			lines.push_back(vec4<float>(start.x, start.y, end.x, end.y));
		}
	}

	while (rdr.clear() && !clicks) {
		rdr.submit(line_info, lines, vec4<float>(1.0f, 1.0f, 1.0f, 1.0f), GL_LINES);
		rdr.draw();
	}
	clicks = 0;

	return Mesh{ std::move(compressed_nodes), std::move(compressed_edges), std::move(compressed_node_cluster_ids) };
}

struct Boundary {
	using Polygon = std::vector<size_t>;
	std::map<size_t, Polygon> polys;
	std::set<id_t> adj;
};
using BoundaryGraph = std::map<id_t, Boundary>;

template <typename T>
std::ostream &operator<<(std::ostream &os, std::set<T> const &s)
{
	os << '{';
	for (const auto &e : s) {
		os << e << ", ";
	}
	os << '}';
	return os;
}

struct SearchEdge {
	size_t s;
	size_t t;

	friend std::ostream &operator<<(std::ostream &os, SearchEdge const &e)
	{
		return os << "(" << e.s << ", " << e.t << ")";
	}
};

auto operator<=>(SearchEdge const &l, SearchEdge const &r)
{
	if (l.s == size_t(-1) || r.s == size_t(-1))
		return l.t <=> r.t;
	if (l.t == size_t(-1) || r.t == size_t(-1))
		return l.s <=> r.s;
	return (l.s == r.s) ? (l.t <=> r.t) : (l.s <=> r.s);
}

auto operator==(SearchEdge const &l, SearchEdge const &r)
{
	if (l.s == size_t(-1) || r.s == size_t(-1))
		return l.t == r.t;
	if (l.t == size_t(-1) || r.t == size_t(-1))
		return l.s == r.s;
	return (l.s == r.s) ? (l.t == r.t) : (l.s == r.s);
}

BoundaryGraph clusterBoundaries(Mesh const &mesh, Render &rdr)
{
	Shader shader(
		ShaderStage(readFile("shdr/line.v.glsl"), ShaderStage::Vertex),
		ShaderStage(readFile("shdr/line.f.glsl"), ShaderStage::Fragment)
	);
	shader.bind();
	shader.set("scale", 1.2f / 24.0f);
	VertexArray va;
	{
		GLuint indices[] = { 0, 1 };
		IndexBuffer(indices).leak();
	}
	VertexBuffer vb(std::span{static_cast<vec4<float>*>(nullptr), 256}, attr2descr{});
	Render::draw_context<attr2descr> ctx{ shader, va, vb, 256 };
	vec4<float> color{ 1.0f, 0.0f, 1.0f, 1.0f };
	std::vector<vec4<float>> lines;
	rdr.removeAll();

	assert(mesh.vert.size() <= mesh.edge.size());
	std::map<id_t, std::map<size_t, std::vector<SearchEdge>>> partial;
	BoundaryGraph bnd;
	using stack = std::deque<size_t>;
	stack dfs;
	// start with anything really
	dfs.push_back(0);
	std::set<size_t> unseen;
	for (size_t seed = 0; seed < mesh.vert.size(); ++seed) {
		unseen.emplace(seed);
	}
	// non connected graph version of dfs
	while (!unseen.empty()) {
		const auto seed = *unseen.begin();
		dfs.push_back(seed);
		while (!dfs.empty()) {
			const auto s = dfs.back();
			dfs.pop_back();
			if (!unseen.contains(s))
				continue;
			for (const auto [t, c1, c2] : mesh.edge[s]) {
				if (!unseen.contains(t))
					continue;
				dfs.push_back(t);
				partial[c1][seed].emplace_back(s, t);
				partial[c2][seed].emplace_back(s, t);
				bnd[c1].adj.emplace(c2);
				bnd[c2].adj.emplace(c1);

				const auto &pos = mesh.vert;
				lines.emplace_back(pos[s].x, pos[s].y, pos[t].x, pos[t].y);
				while (rdr.clear()) {
					rdr.submit(ctx, lines, color, GL_LINES);
					rdr.draw();
					break;
				}
			}
			unseen.erase(s);
		}
	}

	// convert jumbled set of edges into end-to-end edges, represented as array of consecutive vertices
	for (auto &[cluster, boundary] : bnd) {
		for (auto &[seed, source] : partial.find(cluster)->second) {
			auto &dest = boundary.polys[seed];
			auto any = source.begin();
			lines.clear();
			dest.emplace_back(any->s);
			dest.emplace_back(any->t);
			const auto &pos = mesh.vert;
			lines.emplace_back(pos[any->s].x, pos[any->s].y, pos[any->t].x, pos[any->t].y);
			source.erase(source.begin());
			while (!source.empty()) {
				auto next = std::find(source.begin(), source.end(), SearchEdge{dest.back(), size_t(-1)});
				size_t insert;
				if (next == source.end()) {
					next = std::find(source.begin(), source.end(), SearchEdge{size_t(-1), dest.back()});
					assert(next != source.end());
					assert(next->t == dest.back());
					insert = next->s;
				} else {
					assert(next->s == dest.back());
					insert = next->t;
				}
				source.erase(next);
				dest.emplace_back(insert);
				lines.emplace_back(pos[next->s].x, pos[next->s].y, pos[next->t].x, pos[next->t].y);

				while (!clicks && rdr.clear()) {
					rdr.submit(ctx, std::span{lines.begin(), lines.size()-1}, color, GL_LINES);
					rdr.submit(ctx, std::span{lines.end()-1, 1}, vec4<float>(0.0f, 1.0f, 1.0f, 1.0f), GL_LINES);
					rdr.draw();
					break;
				}
				clicks = 0;
			}
		}
	}

	return bnd;
}

bool isInside(const std::vector<vec2<float>>& polygon1, const std::vector<vec2<float>>& polygon2) {
    int intersections = 0;
	vec2<float> point = polygon1[0];
    for (size_t i = 0; i < polygon2.size(); ++i) {
        vec2<float> p1 = polygon2[i];
        vec2<float> p2 = polygon2[(i + 1) % polygon2.size()];

        if (p1.y == p2.y) continue;
        if (point.y < std::min(p1.y, p2.y) || point.y >= std::max(p1.y, p2.y)) continue;

        float x_intersection = (point.y - p1.y) * (p2.x - p1.x) / (p2.y - p1.y) + p1.x;
        if (x_intersection > point.x) {
            intersections++;
        }
    }
    return (intersections % 2) == 1;
}

float calculateSignedPolygonArea(const std::vector<vec2<float>>& polygon_vertices)
{
    if (polygon_vertices.size() < 3) {
        return 0.0;
    }

    float area = 0.0;
    size_t n = polygon_vertices.size();

    for (size_t i = 0; i < n; ++i) {
        size_t j = (i + 1) % n;
        area += (polygon_vertices[i].x * polygon_vertices[j].y);
        area -= (polygon_vertices[j].x * polygon_vertices[i].y);
    }

    return area / 2.0;
}

struct Polygon {
	std::vector<vec2<float>> vertices;
	float signed_area;
	int containment_level = 0;
};

using ClusterGraph = std::unordered_map<id_t, std::vector<Polygon>>;

ClusterGraph clusterGraph(
	const std::vector<std::pair<id_t, std::unordered_map<size_t, std::vector<size_t>>>>& cluster_internal_graphs_vector,
	const std::unordered_map<size_t, std::vector<size_t>>& cluster_nodes,
	const std::vector<vec2<float>>& vert) {

	struct Edge {
		size_t a, b;
		bool operator==(const Edge& other) const {
			return a == other.a && b == other.b;
		}
	};

	struct EdgeHash {
		size_t operator()(const Edge& e) const {
			return std::hash<size_t>()(e.a) * 31 + std::hash<size_t>()(e.b);
		}
	};

	using EdgeSet = std::unordered_set<Edge, EdgeHash>;

	ClusterGraph graph;

	for (int i = 0; i < cluster_internal_graphs_vector.size(); ++i) {

		auto pair_cluster = cluster_internal_graphs_vector[i];
		id_t cluster_id = pair_cluster.first;
        const std::unordered_map<size_t, std::vector<size_t>>& cluster_graph = pair_cluster.second;
        EdgeSet visited_edges_in_cluster;
		std::vector<Polygon> found_polygons;

        for (size_t start_node_idx : cluster_nodes.at(cluster_id)) {
            if (cluster_graph.find(start_node_idx) == cluster_graph.end()) {
                continue;
            }
            
            for (size_t initial_neighbor : cluster_graph.at(start_node_idx)) {
                Edge initial_edge = {std::min(start_node_idx, initial_neighbor), std::max(start_node_idx, initial_neighbor)};
                
                if (visited_edges_in_cluster.count(initial_edge)) {
                    continue;
                }

                std::vector<vec2<float>> polygon_vertices;
                std::vector<size_t> path_nodes;
                
                size_t current_node = start_node_idx;
                size_t previous_node = (size_t)-1;
                size_t entry_node = start_node_idx;

                path_nodes.push_back(current_node);
                polygon_vertices.emplace_back(vert[current_node]);
                
                size_t next_node = initial_neighbor;
                bool cycle_found = false;

                size_t max_path_length = vert.size() * 2; 

                while (path_nodes.size() <= max_path_length) {
                    path_nodes.push_back(next_node);
                    polygon_vertices.emplace_back(vert[next_node]);

					Edge visited_edge_key = {std::min(current_node, next_node), std::max(current_node, next_node)};
					visited_edges_in_cluster.insert(visited_edge_key);
                    
                    previous_node = current_node;
                    current_node = next_node;

                    if (current_node == entry_node && path_nodes.size() >= 3) {
                        cycle_found = true;
                        break;
                    }
                    
                    const auto& neighbors = cluster_graph.at(current_node);
                    size_t best_next_node = (size_t)-1;
                    float max_angle = -1.0;

                    const vec2<float> v_in = vert[current_node] - vert[previous_node];
                    const float angle_in = atan2(v_in.y, v_in.x);

                    for (size_t neighbor : neighbors) {
                        if (neighbor == previous_node) {
                            continue;
                        }

						Edge potential_edge_key = {std::min(current_node, neighbor), std::max(current_node, neighbor)};
						if (visited_edges_in_cluster.count(potential_edge_key) && !(neighbor == entry_node && path_nodes.size() >= 2)) {
							continue;
						}

                        const vec2<float> v_out = vert[neighbor] - vert[current_node];
                        const float angle_out = atan2(v_out.y, v_out.x);
                        
                        float relative_angle = angle_out - angle_in;
                        if (relative_angle < 0) {
                            relative_angle += 2.0 * M_PI;
                        }

                        if (relative_angle > max_angle) {
                            max_angle = relative_angle;
                            best_next_node = neighbor;
                        }
                    }

                    if (best_next_node == (size_t)-1) {
						break; 
                    }
                    next_node = best_next_node;
                }

                if (cycle_found && polygon_vertices.size() >= 3) {
                    Polygon p;
                    p.vertices = polygon_vertices;
                    p.signed_area = calculateSignedPolygonArea(p.vertices);
                    found_polygons.push_back(p);
                }
            }
        }

        graph[cluster_id].insert(graph[cluster_id].end(), found_polygons.begin(), found_polygons.end());

    }

    for (auto& [cluster_id, polygons] : graph) {
        
        for (size_t i = 0; i < polygons.size(); ++i) {
            for (size_t j = 0; j < polygons.size(); ++j) {
                if (i == j) continue;

                if (std::abs(polygons[j].signed_area) > std::abs(polygons[i].signed_area)) {
                    if (isInside(polygons[i].vertices, polygons[j].vertices)) {
                        polygons[i].containment_level++;
                    }
                }
            }
        }
	}

	return graph;
}


std::pair<std::unordered_map<id_t, float>, std::unordered_map<id_t, vec2<float>>> calculateClusterAreas(
	const std::vector<std::pair<id_t, std::unordered_map<size_t, std::vector<size_t>>>>& cluster_internal_graphs_vector,
	const std::unordered_map<size_t, std::vector<size_t>>& cluster_nodes,
	const std::vector<vec2<float>>& vert,
	ClusterGraph& cluster_graph)
{
	std::unordered_map<id_t, float> cluster_total_areas;

	for (auto& [cluster_id, polygons] : cluster_graph) {
		for (auto& p : polygons) {	
			p.signed_area = calculateSignedPolygonArea(p.vertices);
		}
	}

	for (auto& [cluster_id, polygons] : cluster_graph) {	
        float net_area = 0.0f;
        for (const auto& p : polygons) {
            if (p.containment_level % 2 == 1) {
                net_area -= std::abs(p.signed_area);
            } else {
                net_area += std::abs(p.signed_area);
            }
        }
        cluster_total_areas[cluster_id] = net_area;
    }

	std::unordered_map<id_t, vec2<float>> cluster_centers;
	for (const auto& [cluster_id, nodes] : cluster_nodes) {
		vec2<float> sum{0.0, 0.0};
		for (size_t idx : nodes) {
			sum = sum + vert[idx];
		}
		if (!nodes.empty()) {
			sum = sum / static_cast<float>(nodes.size());
		}
		cluster_centers[cluster_id] = sum;
	}

	return std::make_pair(cluster_total_areas, cluster_centers);
	
}

template <buffer_description D>
std::vector<vec2<float>> applyForces(Mesh& mesh, Render &rdr, Render::draw_context<D> const &line_info, float k0, float kN)
{
	std::vector<vec2<float>> vert         = mesh.vert;
	std::vector<std::vector<size_t>> edge(mesh.edge.size());
	for (size_t s = 0; s < mesh.edge.size(); ++s) {
		for (const auto [t, c1, c2] : mesh.edge[s]) {
			edge[s].emplace_back(t);
		}
	}
	std::vector<MaskT>& node_cluster_ids   = mesh.node_cluster_ids;
	const size_t n_nodes                   = node_cluster_ids.size();

	std::unordered_map<size_t, std::vector<size_t>> cluster_nodes;
    for (size_t i = 0; i < n_nodes; ++i) {
        MaskT m = node_cluster_ids[i];
        while (m) {
            int c = __builtin_ctzll(m);
            m &= (m - 1);
            cluster_nodes[c].push_back(i);
		}
    }
	std::unordered_map<size_t, std::set<id_t>> vertex_clusters;
	for (size_t i = 0; i < n_nodes; ++i) {
		MaskT m = node_cluster_ids[i];
		while (m) {
			int c = __builtin_ctzll(m);
			m &= (m - 1);
			vertex_clusters[i].insert(c);
		}
	}

	std::vector<std::set<size_t>> neighbor_map(n_nodes);
	for (size_t u = 0; u < edge.size(); ++u) {
		for (size_t v : edge[u]) {
			if (v < n_nodes) {
				neighbor_map[u].insert(v);
				neighbor_map[v].insert(u);
			}
		}
	}

	std::unordered_map<id_t, std::unordered_map<size_t, std::vector<size_t>>> cluster_internal_graphs;
    for (size_t u = 0; u < edge.size(); ++u) {
        MaskT mu = node_cluster_ids[u];
        for (size_t v : edge[u]) {
            if (v >= n_nodes) continue;
            MaskT common = mu & node_cluster_ids[v];
            while (common) {
                int c = __builtin_ctzll(common);
                common &= (common - 1);
                auto& graph = cluster_internal_graphs[c];
                graph[u].push_back(v);
                graph[v].push_back(u);
            }
        }
    }

	auto draw_current_state = [&](const std::vector<vec2<float>>& vert) {
		std::vector<vec4<float>> lines;
		for (size_t u = 0; u < edge.size(); ++u) {
			for (size_t v : edge[u]) {
				if (v < vert.size()) {
					lines.push_back(vec4<float>(vert[u].x, vert[u].y, vert[v].x, vert[v].y));
				}
			}
		}
		rdr.removeAll();
		rdr.clear();
		rdr.submit(line_info, lines, vec4<float>(1.0f, 0.7f, 0.8f, 1.0f), GL_LINES);
		rdr.draw();
	};

	float force_threshold = 0.001;
	float eta = 0.003;


	std::vector<vec2<float>> vert0 = vert;
	auto cluster_internal_graphs_vector = std::vector<std::pair<id_t, std::unordered_map<size_t, std::vector<size_t>>>>(
		cluster_internal_graphs.begin(), cluster_internal_graphs.end());

	
	ClusterGraph cluster_graph = clusterGraph(cluster_internal_graphs_vector, cluster_nodes, vert);
	auto [areas0, _] = calculateClusterAreas(cluster_internal_graphs_vector, cluster_nodes, vert, cluster_graph);

	float max_force = 2 * force_threshold;
	int it = 0;
	while(max_force > force_threshold)
	{
		max_force = 0.0f;
		it = (it + 1)%1000;

		auto [areas, centers] = calculateClusterAreas(cluster_internal_graphs_vector, cluster_nodes, vert, cluster_graph);

		for (size_t u = 0; u < n_nodes; ++u) {
			vec2<float> force = {0.0, 0.0};

			if (neighbor_map[u].size() == 0) continue;

			// Local Spring
			force = force + (vert0[u] - vert[u]) * k0 * (vert0[u] - vert[u]).length();

			// Neighbor springs
			for (size_t v : neighbor_map[u]) {
				force = force + (vert[v] - vert[u]) * kN;
			}

			// Area forces
			for (id_t c: vertex_clusters[u]){
				float area = areas[c];
				float area0 = areas0[c];
				vec2<float> center = centers[c];

				force = force + (vert[u] - center) * (1.0f - sqrt(area / area0));
			}
			if (force.length() > max_force)
				max_force = force.length();

			vert[u] = vert[u] + force * eta;
		}

		if (it % 1000 == 0) {
			draw_current_state(vert);
		}
	}

	std::cout << "Done applying forces (threshold reached)" << std::endl;

	draw_current_state(vert);
	return vert;
}

std::ostream &operator<<(std::ostream &os, Color color)
{
	char buf[7];
	std::sprintf(buf, "%02x%02x%02x", color.r, color.g, color.b);
	return os << buf;
}

std::string serializeSVG(Color color, Boundary const &bnd, std::vector<vec2<float>> const &pos)
{
	std::stringstream result;
	result << "\t<g fill=\"#" << color << "\">\n";
	for (const auto &[_, outer] : bnd.polys) {
		result << "\t\t<polygon points=\"";
		bool first = true;
		for (const auto i : outer) {
			if (first) {
				first = false;
			} else {
				result << ' ';
			}
			auto out = pos[i] * 25.0f;
			result << out.x << ',' << out.y;
		}
		result << "\"/>\n";
	}
	result << "\t</g>\n";
	return result.str();
}

std::string serializeSVG(Clusters const &clusters, BoundaryGraph const &bnd, std::vector<vec2<float>> const &pos)
{
	std::stringstream result;
	result << R"(<?xml version="1.0"?>)" << '\n';
	result << R"(<svg width="600" height="600" viewBox="-100 -100 700 700" xmlns="http://www.w3.org/2000/svg">)" << '\n';
	std::deque<id_t> dfs;
	// starts from the image boundary
	dfs.push_back(id_t(-1));
	std::set<id_t> seen;
	while (!dfs.empty()) {
		const auto s = dfs.back();
		dfs.pop_back();
		if (seen.contains(s))
			continue;
		seen.emplace(s);
		const auto boundary = bnd.find(s);
		assert(boundary != bnd.end());
		if (s != id_t(-1)) {
			result << serializeSVG(clusters.average_color(s), boundary->second, pos);
		}
		for (const auto &t : boundary->second.adj) {
			dfs.push_back(t);
		}
	}
	result << "</svg>\n";
	return result.str();
}

void writeToFile(std::string_view path, std::string_view content)
{
	std::ofstream file(path.data());
	file << content;
}

int main(int argc, char **argv)
{
	if (argc != 3) {
		std::cout << "Usage: " << argv[0] << " <source> <target>\n";
		return 1;
	}
	Window window("Depixel", 720, 720);
	glfwSetMouseButtonCallback(window.handle, [] (GLFWwindow *, int button, int action, int) {
		if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
			++clicks;
		}
	});
	Render rdr(std::move(window));
	
	// Imgui intialization
	ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
	ImGui::StyleColorsDark();
	ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }
    ImGui_ImplGlfw_InitForOpenGL(rdr.getHandle(), true);
    const char* glsl_version = "#version 330";
    ImGui_ImplOpenGL3_Init(glsl_version);

	Shader shader(
		ShaderStage(
			R"(#version 330 core
			layout(location = 0) in vec2 attr_pos;
			layout(location = 1) in vec2 inst_pos;
			uniform float scale;
			uniform float inner_scale;
			void main() {
				vec2 pos = (attr_pos * inner_scale + inst_pos) * scale;
				pos.y = 1.0 - pos.y;
				gl_Position = vec4(pos - vec2(0.5, 0.5), 0.0, 1.0);
			})", ShaderStage::Vertex),
		ShaderStage(
			R"(#version 330 core
			out vec4 frag_color;
			uniform vec4 color;
			void main() {
				frag_color = color;
			})", ShaderStage::Fragment)
	);
	shader.bind();

	VertexArray va;
	{
		vec2<float> vertices[] = {
			{ 0.0f, 0.0f },
			{ 0.0f,+1.0f },
			{ 1.0f, 0.0f },
			{ 1.0f,+1.0f },
		};
		GLuint indices[] = {
			0, 1, 2,
			2, 1, 3,
		};
		VertexBuffer(vertices, attr0descr{}).leak();
		IndexBuffer(indices).leak();
	}
	size_t npos = 256;
	VertexBuffer vb_pos(std::span{static_cast<vec2<float>*>(nullptr), npos}, attr1descr{});
	Render::draw_context<attr1descr> info{shader, va, vb_pos, npos};

	int width, height, channels;
	stbi_set_flip_vertically_on_load(false);
	auto pixels = stbi_load(argv[1], &width, &height, &channels, 0);
	assert(channels == 3);
	assert(width > 0 && height > 0);

	Clusters clusters(width, height, pixels, 48);
	std::cout << "found " << clusters.components() << " clusters\n";

	vec4<float> colors[] = {
		{ 0.8f, 0.1f, 0.2f, 1.0f },
		{ 0.6f, 0.7f, 0.1f, 1.0f },
		{ 0.2f, 0.9f, 0.4f, 1.0f },
		{ 0.1f, 0.5f, 0.7f, 1.0f },
		{ 0.2f, 0.1f, 0.8f, 1.0f },
	};
	std::vector<std::vector<vec2<float>>> cluster_pos;
	for (const auto &[_, cluster] : clusters.get()) {
		cluster_pos.emplace_back();
		for (const auto &[xy] : cluster) {
			const auto x = xy % width;
			const auto y = xy / width;
			cluster_pos.back().emplace_back(float(x), float(y));
		}
	}

	shader.set("inner_scale", 1.0f);
	shader.set("scale", 1.2f / width);

	Shader line_shader(
		ShaderStage(readFile("shdr/line.v.glsl"), ShaderStage::Vertex),
		ShaderStage(readFile("shdr/line.f.glsl"), ShaderStage::Fragment)
	);
	line_shader.bind();
	line_shader.set("scale", 1.2f / width);

	VertexArray line_va;
	{
		GLuint indices[] = { 0, 1 };
		IndexBuffer(indices).leak();
	}
	size_t nline = 256;
	VertexBuffer line_vb(std::span{static_cast<vec4<float>*>(nullptr), nline}, attr2descr{});
	Render::draw_context<attr2descr> line_info{line_shader, line_va, line_vb, nline};

	for (size_t ic = 0; ic < clusters.components(); ++ic) {
		rdr.submit(info, cluster_pos[ic], colors[ic % std::size(colors)], GL_QUADS);
	}
	rdr.keep();

	Mesh mesh;
	BoundaryGraph bnd;
	std::vector<vec2<float>> smoothed;
	bool built_mesh = false;

	static float k0 = 0.3f;
    static float kN = 0.65f;
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	ImGui::SetNextWindowBgAlpha(0.0f);

	std::vector<vec4<float>> lines;

	while (!glfwWindowShouldClose(rdr.getHandle()))
	{
		rdr.clear();
		glfwPollEvents();
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
		ImGui::Begin("Controls");
		ImGui::Text("Force Parameters");
		ImGui::SliderFloat("k0 (Local Spring Stiffness)", &k0, 0.0f, 1.0f);
		ImGui::SliderFloat("kN (Neighbor Springs Stiffness)", &kN, 0.0f, 1.0f);

		static int delta_c = 48;
		ImGui::SliderInt("delta_c (Cluster Threshold)", &delta_c, 0, 256);

		if (ImGui::Button("Rebuild Clusters"))
		{
			clusters = Clusters(width, height, pixels, delta_c);
			std::cout << "Rebuilt clusters with delta_c = " << delta_c << ", found " << clusters.components() << " clusters\n";

			cluster_pos.clear();
			for (const auto &[_, cluster] : clusters.get()) {
				cluster_pos.emplace_back();
				for (const auto &[xy] : cluster) {
					const auto x = xy % width;
					const auto y = xy / width;
					cluster_pos.back().emplace_back(float(x), float(y));
				}
			}

			rdr.removeAll();
			rdr.clear();
			for (size_t ic = 0; ic < clusters.components(); ++ic) {
				rdr.submit(info, cluster_pos[ic], colors[ic % std::size(colors)], GL_QUADS);
			}
			rdr.keep();
			built_mesh = false;
		}
		if (ImGui::Button("Build Mesh"))
			{
				rdr.removeAll();
				rdr.clear();
				rdr.draw();
				mesh = buildShapes(clusters, width, height, rdr, line_info);
				bnd = clusterBoundaries(mesh, rdr);
				built_mesh = true;
				lines.clear();
				for (size_t u = 0; u < mesh.edge.size(); ++u) {
					for (auto [v, _1, _2] : mesh.edge[u]) {
						if (v < mesh.vert.size()) {
							lines.push_back(vec4<float>(mesh.vert[u].x, mesh.vert[u].y,
														mesh.vert[v].x, mesh.vert[v].y));
						}
					}
				}
				rdr.removeAll();
				rdr.clear();
				rdr.submit(line_info, lines, vec4<float>(1.0f, 0.7f, 0.8f, 1.0f), GL_LINES);
				rdr.keep();
			}
		if (ImGui::Button("Apply Forces"))
		{
			if (built_mesh) {
				smoothed = applyForces(mesh, rdr, line_info, k0, kN);
				std::cout << "applied forces\n";
				auto serialized = serializeSVG(clusters, bnd, smoothed);
				writeToFile(argv[2], serialized);

				lines.clear();
				for (size_t u = 0; u < mesh.edge.size(); ++u) {
					for (auto [v, _1, _2] : mesh.edge[u]) {
						if (v < mesh.vert.size()) {
							lines.push_back(vec4<float>(smoothed[u].x, smoothed[u].y,
														smoothed[v].x, smoothed[v].y));
						}
					}
				}
				rdr.removeAll();
				rdr.clear();
				rdr.submit(line_info, lines, vec4<float>(1.0f, 0.7f, 0.8f, 1.0f), GL_LINES);
				rdr.keep();
			} else {
				std::cout << "You must build shapes before applying forces.\n";
			}
		}
		ImGui::End();

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			GLFWwindow* backup_current_context = glfwGetCurrentContext();
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
			glfwMakeContextCurrent(backup_current_context);
		}
				
		rdr.draw();
		glfwSwapBuffers(rdr.getHandle());
	}

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	stbi_image_free(pixels);

}

