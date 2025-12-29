#include <unordered_map>
#include <unordered_set>

#include <glm/gtc/matrix_inverse.hpp>
#include <spdlog/spdlog.h>

#include "Labs/2-GeometryProcessing/DCEL.hpp"
#include "Labs/2-GeometryProcessing/tasks.h"

namespace VCX::Labs::GeometryProcessing {

#include "Labs/2-GeometryProcessing/marching_cubes_table.h"

    /******************* 1. Mesh Subdivision *****************/
    void SubdivisionMesh(Engine::SurfaceMesh const & input, Engine::SurfaceMesh & output, std::uint32_t numIterations) {
        Engine::SurfaceMesh curr_mesh = input;
        // We do subdivison iteratively.
        for (std::uint32_t it = 0; it < numIterations; ++it) {
            // During each iteration, we first move curr_mesh into prev_mesh.
            Engine::SurfaceMesh prev_mesh;
            prev_mesh.Swap(curr_mesh);
            // Then we create doubly connected edge list.
            DCEL G(prev_mesh);
            if (! G.IsManifold()) {
                spdlog::warn("VCX::Labs::GeometryProcessing::SubdivisionMesh(..): Non-manifold mesh.");
                return;
            }
            // Note that here curr_mesh has already been empty.
            // We reserve memory first for efficiency.
            curr_mesh.Positions.reserve(prev_mesh.Positions.size() * 3 / 2);
            curr_mesh.Indices.reserve(prev_mesh.Indices.size() * 4);
            // Then we iteratively update currently existing vertices.
            for (std::size_t i = 0; i < prev_mesh.Positions.size(); ++i) {
                // Update the currently existing vetex v from prev_mesh.Positions.
                // Then add the updated vertex into curr_mesh.Positions.
                auto v         = G.Vertex(i);
                auto neighbors = v->Neighbors();
                // your code here:
                // v' = (1 - n * u) v + \sum_{i=1}^n u v_i
                // n = 3, u = 3/16, else u = 3 / 8n
                int       n          = neighbors.size();
                float     u          = (n == 3 ? 3.0 / 16 : 3.0 / (8 * n));
                glm::vec3 new_vertex = prev_mesh.Positions[i] * (1 - n * u);
                for (auto neighbor : neighbors)
                    new_vertex += prev_mesh.Positions[neighbor] * u;
                curr_mesh.Positions.push_back(new_vertex);
            }
            // We create an array to store indices of the newly generated vertices.
            // Note: newIndices[i][j] is the index of vertex generated on the "opposite edge" of j-th
            //       vertex in the i-th triangle.
            std::vector<std::array<std::uint32_t, 3U>> newIndices(prev_mesh.Indices.size() / 3, { ~0U, ~0U, ~0U });
            // Iteratively process each halfedge.
            for (auto e : G.Edges()) {
                // newIndices[face index][vertex index] = index of the newly generated vertex
                newIndices[G.IndexOf(e->Face())][e->EdgeLabel()] = curr_mesh.Positions.size();
                auto eTwin                                       = e->TwinEdgeOr(nullptr);
                // eTwin stores the twin halfedge.
                if (! eTwin) {
                    // When there is no twin halfedge (so, e is a boundary edge):
                    // your code here: generate the new vertex and add it into curr_mesh.Positions.
                    // 如果这条边只被一个三角形面所包含（即为边界上的边），则直接取这条边的中点作为新顶点．
                    auto start      = e->From();
                    auto end        = e->To();
                    auto new_vertex = (prev_mesh.Positions[start] + prev_mesh.Positions[end]) / 2.0f;
                    curr_mesh.Positions.push_back(new_vertex);
                } else {
                    // When the twin halfedge exists, we should also record:
                    //     newIndices[face index][vertex index] = index of the newly generated vertex
                    // Because G.Edges() will only traverse once for two halfedges,
                    //     we have to record twice.
                    newIndices[G.IndexOf(eTwin->Face())][e->TwinEdge()->EdgeLabel()] = curr_mesh.Positions.size();
                    // your code here: generate the new vertex and add it into curr_mesh.Positions.
                    auto v_0 = e->From(), v_2 = e->To();
                    auto one_ = (prev_mesh.Positions[v_0] + prev_mesh.Positions[v_2]);

                    auto v_1 = e->OppositeVertex(), v_3 = eTwin->OppositeVertex();
                    auto two_ = (prev_mesh.Positions[v_1] + prev_mesh.Positions[v_3]);

                    auto new_vertex = (one_ * 3.0f + two_ * 1.0f) / 8.0f;

                    curr_mesh.Positions.push_back(new_vertex);
                }
            }

            // Here we've already build all the vertices.
            // Next, it's time to reconstruct face indices.
            for (std::size_t i = 0; i < prev_mesh.Indices.size(); i += 3U) {
                // For each face F in prev_mesh, we should create 4 sub-faces.
                // v0,v1,v2 are indices of vertices in F.
                // m0,m1,m2 are generated vertices on the edges of F.
                auto v0           = prev_mesh.Indices[i + 0U];
                auto v1           = prev_mesh.Indices[i + 1U];
                auto v2           = prev_mesh.Indices[i + 2U];
                auto [m0, m1, m2] = newIndices[i / 3U];
                // Note: m0 is on the opposite edge (v1-v2) to v0.
                // Please keep the correct indices order (consistent with order v0-v1-v2)
                //     when inserting new face indices.
                // toInsert[i][j] stores the j-th vertex index of the i-th sub-face.
                std::uint32_t toInsert[4][3] = {
                    // your code here:
                    { v0, m2, m1 },
                    { v1, m0, m2 },
                    { v2, m1, m0 },
                    { m0, m1, m2 }
                };
                // Do insertion.
                curr_mesh.Indices.insert(
                    curr_mesh.Indices.end(),
                    reinterpret_cast<std::uint32_t *>(toInsert),
                    reinterpret_cast<std::uint32_t *>(toInsert) + 12U);
            }

            if (curr_mesh.Positions.size() == 0) {
                spdlog::warn("VCX::Labs::GeometryProcessing::SubdivisionMesh(..): Empty mesh.");
                output = input;
                return;
            }
        }
        // Update output.
        output.Swap(curr_mesh);
    }

    /******************* 2. Mesh Parameterization *****************/
    void Parameterization(Engine::SurfaceMesh const & input, Engine::SurfaceMesh & output, const std::uint32_t numIterations) {
        // Copy.
        output = input;
        // Reset output.TexCoords.
        output.TexCoords.resize(input.Positions.size(), glm::vec2 { 0 });

        // Build DCEL.
        DCEL G(input);
        if (! G.IsManifold()) {
            spdlog::warn("VCX::Labs::GeometryProcessing::Parameterization(..): non-manifold mesh.");
            return;
        }

        // Set boundary UVs for boundary vertices.
        // your code here: directly edit output.TexCoords
        std::unordered_set<int> edge_points;
        for (int i = 0; i < G.NumOfVertices(); i++) {
            auto vertex = G.Vertex(i);
            if (vertex->OnBoundary()) edge_points.insert(i);
        }
        std::vector<float> weights;
        for (int i = 0; i < G.NumOfVertices(); i++) {
            auto vertex = G.Vertex(i);
            weights.push_back(float(vertex->Neighbors().size()));
        }

        int edge_num      = edge_points.size();
        int edge_per_side = edge_num / 4;

        std::vector<int> edge_points_vec;
        edge_points_vec.push_back(*edge_points.begin());
        for (int i = 1; i < edge_num; ++i) {
            auto it = G.Vertex(edge_points_vec[i - 1])->BoundaryNeighbors();
            if (it.first != edge_points_vec[i - 1])
                edge_points_vec.push_back(it.first);
            else edge_points_vec.push_back(it.second);
        }

        for (int idx = 0; idx < edge_num; ++idx) {
            int   edge_point = edge_points_vec[idx];
            float t;
            if (idx < edge_per_side) {
                t                            = float(idx) / edge_per_side;
                output.TexCoords[edge_point] = glm::vec2(t, 0.0f);
            } else if (idx < 2 * edge_per_side) {
                t                            = float(idx - edge_per_side) / edge_per_side;
                output.TexCoords[edge_point] = glm::vec2(1.0f, t);
            } else if (idx < 3 * edge_per_side) {
                t                            = float(idx - 2 * edge_per_side) / edge_per_side;
                output.TexCoords[edge_point] = glm::vec2(1.0f - t, 1.0f);
            } else {
                t                            = float(idx - 3 * edge_per_side) / edge_per_side;
                output.TexCoords[edge_point] = glm::vec2(0.0f, 1.0f - t);
            }
        }

        // Solve equation via Gauss-Seidel Iterative Method.
        for (int k = 0; k < numIterations; ++k) {
            // your code here:
            for (int i = 0; i < G.NumOfVertices(); i++) {
                if (edge_points.count(i)) continue;
                glm::vec2 sum_uv = { 0.0f, 0.0f };
                for (auto j : G.Vertex(i)->Neighbors()) {
                    sum_uv += output.TexCoords[j];
                }
                output.TexCoords[i] = sum_uv / weights[i];
            }
        }
    }

    /******************* 3. Mesh Simplification *****************/
    void SimplifyMesh(Engine::SurfaceMesh const & input, Engine::SurfaceMesh & output, float simplification_ratio) {
        DCEL G(input);
        if (! G.IsManifold()) {
            spdlog::warn("VCX::Labs::GeometryProcessing::SimplifyMesh(..): Non-manifold mesh.");
            return;
        }
        // We only allow watertight mesh.
        if (! G.IsWatertight()) {
            spdlog::warn("VCX::Labs::GeometryProcessing::SimplifyMesh(..): Non-watertight mesh.");
            return;
        }

        // Copy.
        output = input;

        // Compute Kp matrix of the face f.
        auto UpdateQ {
            [&G, &output](DCEL::Triangle const * f) -> glm::mat4 {
                // your code here:
                auto      v1     = output.Positions[f->VertexIndex(0)];
                auto      v2     = output.Positions[f->VertexIndex(1)];
                auto      v3     = output.Positions[f->VertexIndex(2)];
                auto      normal = glm::normalize(glm::cross(v2 - v1, v3 - v1));
                float     d      = -glm::dot(normal, v1);
                glm::vec4 p(normal, d);
                return glm::outerProduct(p, p);
            }
        };

        // The struct to record contraction info.
        struct ContractionPair {
            DCEL::HalfEdge const * edge;           // which edge to contract; if $edge == nullptr$, it means this pair is no longer valid
            glm::vec4              targetPosition; // the targetPosition $v$ for vertex $edge->From()$ to move to
            float                  cost;           // the cost $v.T * Qbar * v$
        };

        // Given an edge (v1->v2), the positions of its two endpoints (p1, p2) and the Q matrix (Q1+Q2),
        //     return the ContractionPair struct.
        static constexpr auto MakePair {
            [](DCEL::HalfEdge const * edge,
               glm::vec3 const &      p1,
               glm::vec3 const &      p2,
               glm::mat4 const &      Q) -> ContractionPair {
                // your code here:
                glm::vec4 target;
                glm::mat3 Q3(Q);
                if (abs(glm::determinant(Q3)) < 1e-3f) {
                    target = glm::vec4((p1 + p2) * 0.5f, 1.0f);
                } else {
                    glm::vec3 q(Q[0][3], Q[1][3], Q[2][3]);
                    glm::vec3 v = -glm::inverse(Q3) * q;
                    target      = glm::vec4(v, 1.0f);
                }
                float cost = glm::dot(target, Q * target);
                return { edge, target, cost };
            }
        };

        // pair_map: map EdgeIdx to index of $pairs$
        // pairs:    store ContractionPair
        // Qv:       $Qv[idx]$ is the Q matrix of vertex with index $idx$
        // Kf:       $Kf[idx]$ is the Kp matrix of face with index $idx$
        std::unordered_map<DCEL::EdgeIdx, std::size_t> pair_map;
        std::vector<ContractionPair>                   pairs;
        std::vector<glm::mat4>                         Qv(G.NumOfVertices(), glm::mat4(0));
        std::vector<glm::mat4>                         Kf(G.NumOfFaces(), glm::mat4(0));

        // Initially, we compute Q matrix for each faces and it accumulates at each vertex.
        for (auto f : G.Faces()) {
            auto Q = UpdateQ(f);
            Qv[f->VertexIndex(0)] += Q;
            Qv[f->VertexIndex(1)] += Q;
            Qv[f->VertexIndex(2)] += Q;
            Kf[G.IndexOf(f)] = Q;
        }

        pair_map.reserve(G.NumOfFaces() * 3);
        pairs.reserve(G.NumOfFaces() * 3 / 2);

        // Initially, we make pairs from all the contractable edges.
        for (auto e : G.Edges()) {
            if (! G.IsContractable(e)) continue;
            auto v1                            = e->From();
            auto v2                            = e->To();
            auto pair                          = MakePair(e, input.Positions[v1], input.Positions[v2], Qv[v1] + Qv[v2]);
            pair_map[G.IndexOf(e)]             = pairs.size();
            pair_map[G.IndexOf(e->TwinEdge())] = pairs.size();
            pairs.emplace_back(pair);
        }

        // Loop until the number of vertices is less than $simplification_ratio * initial_size$.
        while (G.NumOfVertices() > simplification_ratio * Qv.size()) {
            // Find the contractable pair with minimal cost.
            std::size_t min_idx = ~0;
            for (std::size_t i = 1; i < pairs.size(); ++i) {
                if (! pairs[i].edge) continue;
                if (! ~min_idx || pairs[i].cost < pairs[min_idx].cost) {
                    if (G.IsContractable(pairs[i].edge)) min_idx = i;
                    else pairs[i].edge = nullptr;
                }
            }
            if (! ~min_idx) break;

            // top:    the contractable pair with minimal cost
            // v1:     the reserved vertex
            // v2:     the removed vertex
            // result: the contract result
            // ring:   the edge ring of vertex v1
            ContractionPair & top    = pairs[min_idx];
            auto              v1     = top.edge->From();
            auto              v2     = top.edge->To();
            auto              result = G.Contract(top.edge);
            auto              ring   = G.Vertex(v1)->Ring();

            top.edge             = nullptr;            // The contraction has already been done, so the pair is no longer valid. Mark it as invalid.
            output.Positions[v1] = top.targetPosition; // Update the positions.

            // We do something to repair $pair_map$ and $pairs$ because some edges and vertices no longer exist.
            for (int i = 0; i < 2; ++i) {
                DCEL::EdgeIdx removed           = G.IndexOf(result.removed_edges[i].first);
                DCEL::EdgeIdx collapsed         = G.IndexOf(result.collapsed_edges[i].second);
                pairs[pair_map[removed]].edge   = result.collapsed_edges[i].first;
                pairs[pair_map[collapsed]].edge = nullptr;
                pair_map[collapsed]             = pair_map[G.IndexOf(result.collapsed_edges[i].first)];
            }

            // For the two wing vertices, each of them lose one incident face.
            // So, we update the Q matrix.
            Qv[result.removed_faces[0].first] -= Kf[G.IndexOf(result.removed_faces[0].second)];
            Qv[result.removed_faces[1].first] -= Kf[G.IndexOf(result.removed_faces[1].second)];

            // For the vertex v1, Q matrix should be recomputed.
            // And as the position of v1 changed, all the vertices which are on the ring of v1 should update their Q matrix as well.
            Qv[v1] = glm::mat4(0);
            for (auto e : ring) {
                // your code here:
                //     1. Compute the new Kp matrix for $e->Face()$.
                auto k_p = UpdateQ(e->Face());
                //     2. According to the difference between the old Kp (in $Kf$) and the new Kp (computed in step 1),
                //        update Q matrix of each vertex on the ring (update $Qv$).
                auto k_p_old = Kf[G.IndexOf(e->Face())];
                auto delta_k = k_p - k_p_old;
                Qv[e->From()] += delta_k;
                Qv[e->To()] += delta_k;
                //     3. Update Q matrix of vertex v1 as well (update $Qv$).
                Qv[v1] += k_p;
                //     4. Update $Kf$.
                Kf[G.IndexOf(e->Face())] = k_p;
            }

            // Finally, as the Q matrix changed, we should update the relative $ContractionPair$ in $pairs$.
            // Any pair with the Q matrix of its endpoints changed, should be remade by $MakePair$.
            // your code here:
            for (auto e_tmp : ring) {
                auto p_index = e_tmp->From();
                for (auto f : G.Vertex(p_index)->Faces()) {
                    auto lable = f->LabelOfVertex(p_index);
                    for (int i = 0; i < 3; ++i) {
                        auto e                        = f->Edge(i);
                        auto p1_index                 = e->From();
                        auto p2_index                 = e->To();
                        auto p1                       = output.Positions[p1_index];
                        auto p2                       = output.Positions[p2_index];
                        auto Q                        = Qv[p1_index] + Qv[p2_index];
                        pairs[pair_map[G.IndexOf(e)]] = MakePair(e, p1, p2, Q);
                    }
                }
            }
        }

        // In the end, we check if the result mesh is watertight and manifold.
        if (! G.DebugWatertightManifold()) {
            spdlog::warn("VCX::Labs::GeometryProcessing::SimplifyMesh(..): Result is not watertight manifold.");
        }

        auto exported = G.ExportMesh();
        output.Indices.swap(exported.Indices);
    }

    /******************* 4. Mesh Smoothing *****************/
    void SmoothMesh(Engine::SurfaceMesh const & input, Engine::SurfaceMesh & output, std::uint32_t numIterations, float lambda, bool useUniformWeight) {
        // Define function to compute cotangent value of the angle v1-vAngle-v2
        static constexpr auto GetCotangent =
            [](const glm::vec3 & vAngle, const glm::vec3 & v1, const glm::vec3 & v2) -> float {
            using std::abs;
            using std::atan2;
            using std::clamp;

            glm::dvec3 u = glm::dvec3(v1) - glm::dvec3(vAngle);
            glm::dvec3 v = glm::dvec3(v2) - glm::dvec3(vAngle);

            double dot_uv     = glm::dot(u, v);
            double cross_norm = glm::length(glm::cross(u, v));

            if (cross_norm < 1e-12) {
                return (dot_uv >= 0.0) ? 1e6f : -1e6f;
            }

            double theta = atan2(cross_norm, dot_uv);
            if (theta < 1e-6) {
                return 1e6f; // 角度极小，cot ≈ +∞
            }
            if (abs(theta - M_PI) < 1e-6) {
                return -1e6f; // 接近 π，cot ≈ -∞
            }
            double cot = dot_uv / cross_norm;

            constexpr double COT_MAX = 1e6;
            cot                      = clamp(cot, -COT_MAX, COT_MAX);

            return static_cast<float>(cot);
        };

        DCEL G(input);
        if (! G.IsManifold()) {
            spdlog::warn("VCX::Labs::GeometryProcessing::SmoothMesh(..): Non-manifold mesh.");
            return;
        }
        // We only allow watertight mesh.
        if (! G.IsWatertight()) {
            spdlog::warn("VCX::Labs::GeometryProcessing::SmoothMesh(..): Non-watertight mesh.");
            return;
        }

        Engine::SurfaceMesh prev_mesh;
        prev_mesh.Positions = input.Positions;
        for (std::uint32_t iter = 0; iter < numIterations; ++iter) {
            Engine::SurfaceMesh curr_mesh = prev_mesh;
            for (std::size_t i = 0; i < input.Positions.size(); ++i) {
                // your code here: curr_mesh.Positions[i] = ...
                auto      now_vertex   = G.Vertex(i);
                auto      now_position = prev_mesh.Positions[i];
                glm::vec3 new_position = { 0.0f, 0.0f, 0.0f };
                if (useUniformWeight) {
                    for (auto neighbor : now_vertex->Neighbors()) {
                        new_position += prev_mesh.Positions[neighbor];
                    }
                    new_position /= now_vertex->Neighbors().size();
                } else {
                    float weight_added = 0.0f;
                    for (auto edge : now_vertex->Ring()) {
                        auto neighbor_index = edge->To();
                        auto alpha_index    = edge->From();
                        auto beta_index     = edge->NextEdge()->To();
                        auto neighbor       = prev_mesh.Positions[neighbor_index];
                        auto alpha          = prev_mesh.Positions[alpha_index];
                        auto beta           = prev_mesh.Positions[beta_index];

                        auto cot_alpha = GetCotangent(alpha, now_position, neighbor);
                        auto cot_beta  = GetCotangent(beta, now_position, neighbor);

                        auto wij = cot_alpha + cot_beta;
                        weight_added += wij;
                        new_position += neighbor * wij;
                    }
                    if (abs(weight_added) <= 1e-6) {
                        new_position = now_position;
                    }
                    new_position /= weight_added;
                }
                curr_mesh.Positions[i] = ((1 - lambda) * now_position) + (lambda * new_position);
            }
            // Move curr_mesh to prev_mesh.
            prev_mesh.Swap(curr_mesh);
        }
        // Move prev_mesh to output.
        output.Swap(prev_mesh);
        // Copy indices from input.
        output.Indices = input.Indices;
    }

    /******************* 5. Marching Cubes *****************/
    void MarchingCubes(Engine::SurfaceMesh & output, const std::function<float(const glm::vec3 &)> & sdf, const glm::vec3 & grid_min, const float dx, const int n) {
        // your code here:
        std::map<int, std::pair<int, int>> edge_point = {
            {  0, { 0, 1 } },
            {  1, { 2, 3 } },
            {  2, { 4, 5 } },
            {  3, { 6, 7 } },
            {  4, { 0, 2 } },
            {  5, { 4, 6 } },
            {  6, { 1, 3 } },
            {  7, { 5, 7 } },
            {  8, { 0, 4 } },
            {  9, { 1, 5 } },
            { 10, { 2, 6 } },
            { 11, { 3, 7 } }
        };
        auto edge_id = [n](int dir, int i, int j, int k) -> long long {
            long long base = 0;
            if (dir == 0) {
                base = 0;
                return base + (k * n + j) * (n + 1) + i;
            } else if (dir == 1) {
                base = (long long) (n) * (n + 1) * n;
                return base + (k * (n + 1) + j) * n + i;
            } else if (dir == 2) {
                base = 2LL * (long long) (n) * (n + 1) * n;
                return base + (k * n + j) * n + i;
            }
            return -1;
        };
        std::unordered_map<long long, int> pos_index;
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                for (int k = 0; k < n; k++) {
                    glm::vec3              start      = grid_min + glm::vec3 { dx * i, dx * j, dx * k };
                    unsigned               cube_index = 0;
                    std::vector<glm::vec3> all_cube_points;
                    for (int mask = 0; mask < 8; mask++) {
                        glm::vec3 now = start + glm::vec3 { (mask & 1) * dx, ((mask >> 1) & 1) * dx, ((mask >> 2) & 1) * dx };
                        cube_index |= sdf(now) > 0 ? (1 << mask) : 0;
                        all_cube_points.push_back(now);
                    }
                    auto             it = c_EdgeStateTable[cube_index];
                    std::vector<int> point_index(12);
                    for (int mask = 0; mask < 12; mask++) {
                        if ((it >> mask) & 1) {
                            auto [i1, i2]   = edge_point[mask];
                            int       now_i = i + (i1 & 1), now_j = j + ((i1 >> 1) & 1), now_k = k + ((i1 >> 2) & 1);
                            long long now_edge_id = edge_id(mask / 4, now_i, now_j, now_k);
                            if (pos_index.find(now_edge_id) != pos_index.end()) {
                                point_index[mask] = pos_index[now_edge_id];
                            } else {
                                glm::vec3 insert_point;
                                glm::vec3 p1 = all_cube_points[i1], p2 = all_cube_points[i2];
                                float     v1 = sdf(p1), v2 = sdf(p2);
                                float     t            = v1 / (v1 - v2); // in [0,1]
                                insert_point           = p1 + t * (p2 - p1);
                                point_index[mask]      = output.Positions.size();
                                pos_index[now_edge_id] = output.Positions.size();
                                output.Positions.push_back(insert_point);
                            }
                        }
                    }
                    auto edge_link = c_EdgeOrdsTable[cube_index];
                    for (int tri_idx = 0; tri_idx <= 4; tri_idx++) {
                        if (edge_link[3 * tri_idx] < 0)
                            break;
                        int e0 = edge_link[3 * tri_idx], e1 = edge_link[3 * tri_idx + 1], e2 = edge_link[3 * tri_idx + 2];
                        output.Indices.push_back(point_index[e0]);
                        output.Indices.push_back(point_index[e1]);
                        output.Indices.push_back(point_index[e2]);
                    }
                }
            }
        }
    }
} // namespace VCX::Labs::GeometryProcessing
