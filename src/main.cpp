#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <iostream>
#include <format>
#include <cassert>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <cstdint>
#include <thread>
#include <chrono>
#include "data.hpp"
#include "gfx.hpp"
#include "gfx/shader.hpp"
#include "gfx/buffer.hpp"
#include "gfx/render.hpp"

using MaskT = uint64_t; 

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
	std::vector<vec2<float>> vert;
	std::vector<std::vector<size_t>> edge;
	std::vector<MaskT> node_cluster_ids;
};

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
	std::map<size_t, std::vector<size_t>> edges;

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
						edges[edge_node_end].push_back(edge_node_end+1);
					}
					node_cluster_ids[edge_node_end] |= (1ULL << cluster_to_compact[current]);
					nodes[edge_node_end++] = lerp(start, end, t);
				}
				nodes.emplace_back(start);
				nodes.emplace_back(end  );
				node_cluster_ids.emplace_back(1ULL << cluster_to_compact[current]);
				node_cluster_ids.emplace_back(1ULL << cluster_to_compact[current]);
				edges[edge_node_end-ARITY].push_back(nodes.size()-2);
				edges[edge_node_end-1    ].push_back(nodes.size()-1);
				corners.push_back(choice{ x, y, o });
			}
		}
	}

	std::vector<vec4<float>> lines;
	for (const auto &[s, neighbrs] : edges) {
		for (const auto t : neighbrs) {
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
	for (id_t i = 0; i < node_map.data.size(); ++i) {
		compress.try_emplace(node_map.find(i), compress.size());
	}

	lines.clear();
	std::vector<vec2<float>> compressed_nodes(compress.size());
	std::vector<MaskT> compressed_node_cluster_ids(compress.size());
	std::map<size_t, std::set<size_t>> remapped_edges;
	for (const auto [orig, mapped] : compress) {
		compressed_nodes[mapped] = nodes[orig];
		compressed_node_cluster_ids[mapped] = node_cluster_ids[orig];
	}
	for (const auto [orig, mapped] : compress) {
		for (const auto neighbr : edges[orig]) {
			const auto mapped_neighbr = compress.find(node_map.find(neighbr));
			assert(mapped_neighbr != compress.end());
			const auto [min, max] = std::minmax(mapped, mapped_neighbr->second);
			assert(min != max);
			remapped_edges[min].emplace(max);
			const auto start = compressed_nodes[min];
			const auto end   = compressed_nodes[max];
			lines.push_back(vec4<float>(start.x, start.y, end.x, end.y));
		}
	}

	rdr.clear();
	rdr.submit(line_info, lines, vec4<float>(1.0f, 1.0f, 1.0f, 1.0f), GL_LINES);
	rdr.draw();

	lines.clear();
	std::vector<std::vector<size_t>> compressed_edges(remapped_edges.size());
	for (const auto &[s, neighbrs] : remapped_edges) {
		for (const auto t : neighbrs) {
			compressed_edges[s].push_back(t);
			const auto start = compressed_nodes[s];
			const auto end   = compressed_nodes[t];
			lines.push_back(vec4<float>(start.x, start.y, end.x, end.y));
		}
	}

	while (rdr.clear()) {
		rdr.submit(line_info, lines, vec4<float>(1.0f, 1.0f, 1.0f, 1.0f), GL_LINES);
		rdr.draw();
		break;
	}

	return Mesh{ std::move(compressed_nodes), std::move(compressed_edges), std::move(compressed_node_cluster_ids) };
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

std::pair<std::map<id_t, float>, std::map<id_t, vec2<float>>> calculateClusterAreas(
	std::map<id_t, std::map<size_t, std::vector<size_t>>>& cluster_internal_graphs,
	std::map<size_t, std::vector<size_t>>& cluster_nodes,
	std::vector<vec2<float>>& vert)
{
	std::map<id_t, float> cluster_total_areas;
	for (const auto& pair_cluster : cluster_internal_graphs) {
        id_t cluster_id = pair_cluster.first;
        const std::map<size_t, std::vector<size_t>>& cluster_graph = pair_cluster.second;
        std::set<std::pair<size_t, size_t>> visited_edges_in_cluster;
        for (size_t start_node_idx : cluster_nodes[cluster_id]) {
            if (cluster_graph.find(start_node_idx) == cluster_graph.end()) {
                continue;
            }
            
            for (size_t initial_neighbor : cluster_graph.at(start_node_idx)) {
                std::pair<size_t, size_t> initial_edge = {std::min(start_node_idx, initial_neighbor), std::max(start_node_idx, initial_neighbor)};
                
                if (visited_edges_in_cluster.count(initial_edge)) {
                    continue;
                }

                std::vector<vec2<float>> polygon_vertices;
                std::vector<size_t> path_nodes;
                
                size_t current_node = start_node_idx;
                size_t previous_node = (size_t)-1;
                size_t entry_node = start_node_idx;

                path_nodes.push_back(current_node);
                polygon_vertices.push_back(vert[current_node]);
                
                size_t next_node = initial_neighbor;
                bool cycle_found = false;

                size_t max_path_length = vert.size() * 2; 

                while (path_nodes.size() <= max_path_length) {
                    path_nodes.push_back(next_node);
                    polygon_vertices.push_back(vert[next_node]);

                    std::pair<size_t, size_t> visited_edge_key = {std::min(current_node, next_node), std::max(current_node, next_node)};
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

                        std::pair<size_t, size_t> potential_edge_key = {std::min(current_node, neighbor), std::max(current_node, neighbor)};
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
                    cluster_total_areas[cluster_id] += std::abs(calculateSignedPolygonArea(polygon_vertices));
                }
            }
        }
    }

    for (const auto& [cluster_id, area] : cluster_total_areas) {
        cluster_total_areas[cluster_id] = std::abs(area);
    }

	std::map<id_t, vec2<float>> cluster_centers;
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
void applyForces(Mesh& mesh, Render &rdr, Render::draw_context<D> const &line_info)
{
	std::vector<vec2<float>> vert         = mesh.vert;
    std::vector<std::vector<size_t>> edge = mesh.edge;
	std::vector<MaskT> node_cluster_ids   = mesh.node_cluster_ids;
	const size_t n_nodes                  = node_cluster_ids.size();

	std::map<size_t, std::vector<size_t>> cluster_nodes;
    for (size_t i = 0; i < n_nodes; ++i) {
        MaskT m = node_cluster_ids[i];
        while (m) {
            int c = __builtin_ctzll(m);
            m &= (m - 1);
            cluster_nodes[c].push_back(i);
		}
    }
	std::map<size_t, std::set<id_t>> vertex_clusters;
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

	std::map<id_t, std::map<size_t, std::vector<size_t>>> cluster_internal_graphs;
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

	auto draw_current_state = [&](const std::vector<vec2<float>>& vert, bool stay_visible) {
		std::vector<vec4<float>> lines;
		for (size_t u = 0; u < edge.size(); ++u) {
			for (size_t v : edge[u]) {
				if (v < vert.size()) {
					lines.push_back(vec4<float>(vert[u].x, vert[u].y, vert[v].x, vert[v].y));
				}
			}
		}
		rdr.removeAll();
		while (rdr.clear() && stay_visible) {
			rdr.submit(line_info, lines, vec4<float>(1.0f, 0.7f, 0.8f, 1.0f), GL_LINES);
			rdr.draw();
		}
	};

	float force_threshold = 0.01;
	float k0 = 0.15;
	float kN = 0.15;
	float eta = 0.0015;

	std::vector<vec2<float>> vert0 = vert;
	auto [areas0, _] = calculateClusterAreas(cluster_internal_graphs, cluster_nodes, vert);

	float max_force = 2 * force_threshold;
	int it = 0;
	while(max_force > force_threshold)
	{
		max_force = 2 * force_threshold;
		it = (it + 1)%1000;

		auto [areas, centers] = calculateClusterAreas(cluster_internal_graphs, cluster_nodes, vert);

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
				max_force = force.length();

			vert[u] = vert[u] + force * eta;
		}

		if (it % 1000 == 0)
			draw_current_state(vert, false);

		// std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	std::cout << "Done applying forces (threshold reached)" << std::endl;

	draw_current_state(vert, true);
}

int main(int argc, char **argv)
{
	if (argc != 3) {
		std::cout << "Usage: " << argv[0] << " <source> <target>\n";
		return 1;
	}
	Window window("Depixel", 720, 720);
	Render rdr(std::move(window));

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

	Clusters clusters(width, height, pixels);
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
		ShaderStage(R"(#version 330 core
			layout(location = 2) in vec4 inst_endpoints;
			uniform float scale;
			void main() {
				vec2 pos = inst_endpoints.xy;
				if ((gl_VertexID & 1) == 1) {
					pos = inst_endpoints.zw;
				}
				pos = pos * scale;
				pos.y = 1.0 - pos.y;
				gl_Position = vec4(pos - vec2(0.5, 0.5), 0.0, 1.0);
			})", ShaderStage::Vertex),
		ShaderStage(R"(#version 330 core
			out vec4 frag_color;
			uniform vec4 color;
			void main() {
				frag_color = color;
			})", ShaderStage::Fragment)
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

	auto mesh = buildShapes(clusters, width, height, rdr, line_info);
	applyForces(mesh, rdr, line_info);

	stbi_image_free(pixels);
}

