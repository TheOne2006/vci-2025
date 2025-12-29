#pragma once

#include <numeric>
#include <stack>
#include <spdlog/spdlog.h>

#include "Engine/Scene.h"
#include "Labs/3-Rendering/Ray.h"

namespace VCX::Labs::Rendering {

    constexpr float EPS1 = 1e-2f; // distance to prevent self-intersection
    constexpr float EPS2 = 1e-8f; // angle for parallel judgement
    constexpr float EPS3 = 1e-4f; // relative distance to enlarge kdtree

    glm::vec4 GetTexture(Engine::Texture2D<Engine::Formats::RGBA8> const & texture, glm::vec2 const & uvCoord);

    glm::vec4 GetAlbedo(Engine::Material const & material, glm::vec2 const & uvCoord);

    struct Intersection {
        float t, u, v; // ray parameter t, barycentric coordinates (u, v)
    };

    bool IntersectTriangle(Intersection & output, Ray const & ray, glm::vec3 const & p1, glm::vec3 const & p2, glm::vec3 const & p3);

    struct RayHit {
        bool              IntersectState;
        Engine::BlendMode IntersectMode;
        glm::vec3         IntersectPosition;
        glm::vec3         IntersectNormal;
        glm::vec4         IntersectAlbedo;   // [Albedo   (vec3), Alpha     (float)]
        glm::vec4         IntersectMetaSpec; // [Specular (vec3), Shininess (float)]
    };

    struct TrivialRayIntersector {
        Engine::Scene const * InternalScene = nullptr;

        TrivialRayIntersector() = default;

        void InitScene(Engine::Scene const * scene) {
            InternalScene = scene;
        }

        RayHit IntersectRay(Ray const & ray) const {
            RayHit result;
            if (! InternalScene) {
                spdlog::warn("VCX::Labs::Rendering::RayIntersector::IntersectRay(..): uninitialized intersector.");
                result.IntersectState = false;
                return result;
            }
            int          modelIdx, meshIdx;
            Intersection its;
            float        tmin     = 1e7, umin, vmin;
            int          maxmodel = InternalScene->Models.size();
            for (int i = 0; i < maxmodel; ++i) {
                auto const & model  = InternalScene->Models[i];
                int          maxidx = model.Mesh.Indices.size();
                for (int j = 0; j < maxidx; j += 3) {
                    std::uint32_t const * face = model.Mesh.Indices.data() + j;
                    glm::vec3 const &     p1   = model.Mesh.Positions[face[0]];
                    glm::vec3 const &     p2   = model.Mesh.Positions[face[1]];
                    glm::vec3 const &     p3   = model.Mesh.Positions[face[2]];
                    if (! IntersectTriangle(its, ray, p1, p2, p3)) continue;
                    if (its.t < EPS1 || its.t > tmin) continue;
                    tmin = its.t, umin = its.u, vmin = its.v, modelIdx = i, meshIdx = j;
                }
            }
            if (tmin == 1e7) {
                result.IntersectState = false;
                return result;
            }
            auto const &          model     = InternalScene->Models[modelIdx];
            auto const &          normals   = model.Mesh.IsNormalAvailable() ? model.Mesh.Normals : model.Mesh.ComputeNormals();
            auto const &          texcoords = model.Mesh.IsTexCoordAvailable() ? model.Mesh.TexCoords : model.Mesh.GetEmptyTexCoords();
            std::uint32_t const * face      = model.Mesh.Indices.data() + meshIdx;
            glm::vec3 const &     p1        = model.Mesh.Positions[face[0]];
            glm::vec3 const &     p2        = model.Mesh.Positions[face[1]];
            glm::vec3 const &     p3        = model.Mesh.Positions[face[2]];
            glm::vec3 const &     n1        = normals[face[0]];
            glm::vec3 const &     n2        = normals[face[1]];
            glm::vec3 const &     n3        = normals[face[2]];
            glm::vec2 const &     uv1       = texcoords[face[0]];
            glm::vec2 const &     uv2       = texcoords[face[1]];
            glm::vec2 const &     uv3       = texcoords[face[2]];
            result.IntersectState           = true;
            auto const & material           = InternalScene->Materials[model.MaterialIndex];
            result.IntersectMode            = material.Blend;
            result.IntersectPosition        = (1.0f - umin - vmin) * p1 + umin * p2 + vmin * p3;
            result.IntersectNormal          = (1.0f - umin - vmin) * n1 + umin * n2 + vmin * n3;
            glm::vec2 uvCoord               = (1.0f - umin - vmin) * uv1 + umin * uv2 + vmin * uv3;
            result.IntersectAlbedo          = GetAlbedo(material, uvCoord);
            result.IntersectMetaSpec        = GetTexture(material.MetaSpec, uvCoord);

            return result;
        }
    };

    /* Optional: write your own accelerated intersector here */
    struct WTXYRayIntersector {
        WTXYRayIntersector() = default;

        Engine::Scene const * InternalScene = nullptr;
        // A struct to hold information about each primitive (triangle) for BVH construction.
        struct BVHObject {
            using AABB = std::pair<glm::vec3, glm::vec3>;
            struct PrimitiveInfo {
                glm::vec3 p1, p2, p3;
                glm::vec3 centroid;
                int       meshIdx;
                AABB      AABB;
            };
            // The BVH node structure.
            struct BVHNode {
                AABB             AABB;
                std::vector<int> primitives_id;
                int              lchild, rchild, now_id;
            };

            int                        modelIdx;
            std::vector<BVHNode>       _nodes;
            std::vector<PrimitiveInfo> _primitives;

            // RayAABB
            bool RayAABB(const Ray & ray, const AABB & box, float & t_near, float & t_far) const {
                t_near = -std::numeric_limits<float>::infinity();
                t_far  = std::numeric_limits<float>::infinity();
                for (int i = 0; i < 3; ++i) {
                    if (std::abs(ray.Direction[i]) < EPS2) {
                        if (ray.Origin[i] < box.first[i] || ray.Origin[i] > box.second[i]) return false;
                    } else {
                        float t1 = (box.first[i] - ray.Origin[i]) / ray.Direction[i];
                        float t2 = (box.second[i] - ray.Origin[i]) / ray.Direction[i];
                        if (t1 > t2) std::swap(t1, t2);
                        t_near = std::max(t_near, t1);
                        t_far  = std::min(t_far, t2);
                        if (t_near > t_far) return false;
                    }
                }
                return true;
            }

            // update AABB
            void update_AABB(AABB & now_AABB, const glm::vec3 & p) const {
                now_AABB.first.x = std::min(now_AABB.first.x, p.x);
                now_AABB.first.y = std::min(now_AABB.first.y, p.y);
                now_AABB.first.z = std::min(now_AABB.first.z, p.z);

                now_AABB.second.x = std::max(now_AABB.second.x, p.x);
                now_AABB.second.y = std::max(now_AABB.second.y, p.y);
                now_AABB.second.z = std::max(now_AABB.second.z, p.z);
            }

            // init AABB
            void init_AABB(AABB & box) const {
                box.first  = glm::vec3(std::numeric_limits<float>::infinity());
                box.second = glm::vec3(-std::numeric_limits<float>::infinity());
            }

            // merge AABB
            AABB merge_AABB(const AABB & a, const AABB & b) const {
                AABB result;
                result.first.x = std::min(a.first.x, b.first.x);
                result.first.y = std::min(a.first.y, b.first.y);
                result.first.z = std::min(a.first.z, b.first.z);

                result.second.x = std::max(a.second.x, b.second.x);
                result.second.y = std::max(a.second.y, b.second.y);
                result.second.z = std::max(a.second.z, b.second.z);

                return result;
            }

            // AABB area
            float get_AABB_area(const AABB & a) const {
                glm::vec3 extent = a.second - a.first;
                return 2.0f * (extent.x * extent.y + extent.x * extent.z + extent.y * extent.z);
            }

            // Initializes the intersector by building the BVH Tree for the given Model.
            void InitModel(Engine::Scene const * scene, int model_id) {
                modelIdx = model_id;
                if (! scene || scene->Models.empty()) return;
                // Step 1: Flatten all triangles from all models into a single primitive list.
                _primitives.clear(), _nodes.clear();
                AABB root_AABB;
                init_AABB(root_AABB);
                auto const & model  = scene->Models[model_id];
                int          maxidx = model.Mesh.Indices.size();
                for (int j = 0; j < maxidx; j += 3) {
                    std::uint32_t const * face = model.Mesh.Indices.data() + j;
                    glm::vec3 const &     p1   = model.Mesh.Positions[face[0]];
                    glm::vec3 const &     p2   = model.Mesh.Positions[face[1]];
                    glm::vec3 const &     p3   = model.Mesh.Positions[face[2]];
                    AABB                  now_AABB;
                    init_AABB(now_AABB);
                    update_AABB(now_AABB, p1),
                        update_AABB(now_AABB, p2),
                        update_AABB(now_AABB, p3);
                    // Expand the triangle AABB slightly to account for numerical issues (relative growth by EPS3)
                    glm::vec3 extent = now_AABB.second - now_AABB.first;
                    glm::vec3 grow   = extent * EPS3;
                    now_AABB.first  -= grow;
                    now_AABB.second += grow;
                    _primitives.push_back({ p1, p2, p3, (p1 + p2 + p3) / 3.0f, j, now_AABB });
                    update_AABB(root_AABB, p1),
                        update_AABB(root_AABB, p2),
                        update_AABB(root_AABB, p3);
                }
                // Step 2: Build the BVH tree recursively.
                _nodes.reserve(_primitives.size() * 2); // Pre-allocate memory for efficiency
                std::vector<int> rootPrims(_primitives.size());
                std::iota(rootPrims.begin(), rootPrims.end(), 0);
                BVHNode root = { root_AABB, rootPrims, -1, -1, 0 };
                _nodes.emplace_back(root);
                printf("Triangle nums: %ld\n", _primitives.size());
                buildBVHTree(_nodes[0]);
            }

            // Recursively builds the BVH tree using Surface Area Heuristic (SAH).
            void buildBVHTree(BVHNode & now_node) {
                // if the number of primitives is small, make it a leaf node.
                if (now_node.primitives_id.size() <= 4) return;

                float min_cost  = std::numeric_limits<float>::infinity();
                int   best_axis = -1, best_split_idx = -1;
                float total_SA = get_AABB_area(now_node.AABB); // total area
                for (int axis = 0; axis < 3; ++axis) {
                    std::sort(now_node.primitives_id.begin(), now_node.primitives_id.end(), [&](int a, int b) {
                        return _primitives[a].centroid[axis] < _primitives[b].centroid[axis];
                    });

                    // Precompute AABBs from left and right for efficient cost calculation.
                    std::vector<AABB> left_AABBs(now_node.primitives_id.size());
                    std::vector<AABB> right_AABBs(now_node.primitives_id.size());
                    AABB              l_AABB, r_AABB;
                    init_AABB(l_AABB);
                    init_AABB(r_AABB);

                    // Precompute All AABB
                    for (int i = 0; i < now_node.primitives_id.size(); ++i) {
                        l_AABB        = merge_AABB(l_AABB, _primitives[now_node.primitives_id[i]].AABB);
                        left_AABBs[i] = l_AABB;
                    }
                    for (int i = now_node.primitives_id.size() - 1; i >= 0; --i) {
                        r_AABB         = merge_AABB(r_AABB, _primitives[now_node.primitives_id[i]].AABB);
                        right_AABBs[i] = r_AABB;
                    }

                    // Evaluate SAH cost for each potential split.
                    for (int i = 1; i < now_node.primitives_id.size(); ++i) {
                        l_AABB     = left_AABBs[i - 1];
                        r_AABB     = right_AABBs[i];
                        float l_SA = get_AABB_area(l_AABB);
                        float r_SA = get_AABB_area(r_AABB);
                        float cost = (l_SA * i + r_SA * (now_node.primitives_id.size() - i)) / total_SA;
                        if (cost < min_cost)
                            min_cost = cost, best_axis = axis, best_split_idx = i;
                    }
                }

                float no_split_cost = now_node.primitives_id.size();
                if (min_cost >= no_split_cost || best_axis == -1) return;

                std::sort(now_node.primitives_id.begin(), now_node.primitives_id.end(), [&](int a, int b) {
                    return _primitives[a].centroid[best_axis] < _primitives[b].centroid[best_axis];
                });

                std::vector<int> l_prims, r_prims;
                AABB             l_AABB, r_AABB;
                init_AABB(l_AABB), init_AABB(r_AABB);

                for (int i = 0; i < best_split_idx; ++i) {
                    l_prims.push_back(now_node.primitives_id[i]);
                    l_AABB = merge_AABB(l_AABB, _primitives[now_node.primitives_id[i]].AABB);
                }
                for (int i = best_split_idx; i < now_node.primitives_id.size(); ++i) {
                    r_prims.push_back(now_node.primitives_id[i]);
                    r_AABB = merge_AABB(r_AABB, _primitives[now_node.primitives_id[i]].AABB);
                }

                // create subnode
                now_node.lchild = _nodes.size();
                _nodes.emplace_back(BVHNode { l_AABB, l_prims, -1, -1, int(_nodes.size()) });
                now_node.rchild = _nodes.size();
                _nodes.emplace_back(BVHNode { r_AABB, r_prims, -1, -1, int(_nodes.size()) });
                now_node.primitives_id.clear();
                now_node.primitives_id.shrink_to_fit();
                buildBVHTree(_nodes[now_node.lchild]);
                buildBVHTree(_nodes[now_node.rchild]);
            }
            struct RayHitObject {
                bool  IntersectState;
                float tmin_global;
                float umin, vmin;
                int   hitMeshIdx;
                int   hitModelIdx;
            };

            // Intersects a ray with the scene using the BVH tree.
            RayHitObject IntersectRayInObject(Ray const & ray, float now_tmin) const {
                RayHitObject result;
                result.IntersectState = false;
                result.hitModelIdx    = modelIdx;
                result.hitMeshIdx     = -1;
                if (_nodes.empty()) {
                    spdlog::warn("Not Initialize");
                    result.IntersectState = false;
                    return result;
                }
                result.tmin_global = now_tmin;
                result.umin = 0.0f, result.vmin = 0.0f;

                std::stack<int> stack;
                stack.push(0); // Start with the root node

                while (! stack.empty()) {
                    int nodeId = stack.top();
                    stack.pop();
                    const BVHNode & node = _nodes[nodeId];
                    // Ray-AABB intersection test
                    float t_near        = -std::numeric_limits<float>::infinity();
                    float t_far         = std::numeric_limits<float>::infinity();
                    bool  intersectAABB = true;

                    if (! RayAABB(ray, node.AABB, t_near, t_far) || t_near > result.tmin_global)
                        continue;

                    // If it's a leaf node, intersect with primitives
                    if (node.lchild == -1 && node.rchild == -1) {
                        for (int prim_id : node.primitives_id) {
                            const auto & prim = _primitives[prim_id];
                            Intersection its;
                            if (IntersectTriangle(its, ray, prim.p1, prim.p2, prim.p3)) {
                                if (its.t > EPS1 && its.t <= result.tmin_global) {
                                    result.tmin_global = its.t;
                                    result.umin        = its.u;
                                    result.vmin        = its.v;
                                    result.hitMeshIdx  = prim.meshIdx;
                                }
                            }
                        }
                    } else { // If it's an internal node, push children to stack
                        const BVHNode & leftChild  = _nodes[node.lchild];
                        const BVHNode & rightChild = _nodes[node.rchild];

                        float tNearLeft, tFarLeft;
                        float tNearRight, tFarRight;

                        // ray-AABB for left
                        if (! RayAABB(ray, leftChild.AABB, tNearLeft, tFarLeft) || tFarLeft < 0 || tNearLeft > result.tmin_global)
                            tNearLeft = std::numeric_limits<float>::infinity();
                        // ray-AABB for right
                        if (! RayAABB(ray, rightChild.AABB, tNearRight, tFarRight) || tFarRight < 0 || tNearRight > result.tmin_global)
                            tNearRight = std::numeric_limits<float>::infinity();

                        // Push in reverse order: far first, near later
                        if (tNearLeft < tNearRight) {
                            if (tNearRight != std::numeric_limits<float>::infinity()) stack.push(node.rchild);
                            if (tNearLeft != std::numeric_limits<float>::infinity()) stack.push(node.lchild);
                        } else {
                            if (tNearLeft != std::numeric_limits<float>::infinity()) stack.push(node.lchild);
                            if (tNearRight != std::numeric_limits<float>::infinity()) stack.push(node.rchild);
                        }
                    }
                }
                result.IntersectState = (result.hitMeshIdx != -1);
                return result;
            }
        };

        std::vector<BVHObject> objects;

        // Initializes the intersector by building the BVH Tree for the given scene.
        void InitScene(Engine::Scene const * scene) {
            InternalScene = scene;
            if (! InternalScene || InternalScene->Models.empty()) return;
            for (int i = 0; i < scene->Models.size(); i++) {
                objects.emplace_back();
                objects[i].InitModel(scene, i);
            }
        }

        // Intersects a ray with the scene using the BVH tree.
        RayHit IntersectRay(Ray const & ray) const {
            std::vector<BVHObject::RayHitObject> results;
            RayHit                               result;
            result.IntersectState = false;
            if (! InternalScene) {
                spdlog::warn("VCX::Labs::Rendering::RayIntersector::IntersectRay(..): uninitialized intersector.");
                result.IntersectState = false;
                return result;
            }
            float tmin_global = std::numeric_limits<float>::infinity();
            int   hitModelIdx = -1, hitMeshIdx = -1;
            float umin = 0.0f, vmin = 0.0f;

            for (int i = 0; i < InternalScene->Models.size(); i++) {
                auto model_result = objects[i].IntersectRayInObject(ray, tmin_global);
                if (model_result.IntersectState) {
                    if (model_result.tmin_global > EPS1 && model_result.tmin_global <= tmin_global) {
                        if (fabs(model_result.tmin_global - tmin_global) < EPS1) {
                            results.push_back(model_result);
                        } else {
                            results.clear();
                            results.push_back(model_result);
                        }
                        tmin_global = model_result.tmin_global;
                        umin        = model_result.umin;
                        vmin        = model_result.vmin;
                        hitMeshIdx  = model_result.hitMeshIdx;
                        hitModelIdx = model_result.hitModelIdx;
                    }
                }
            }

            if (hitModelIdx == -1) {
                return result;
            }

            if (results.size() > 1) {
                // empty implementation
            }

            // If an intersection was found, compute the detailed hit information.
            auto const &          model     = InternalScene->Models[hitModelIdx];
            auto const &          normals   = model.Mesh.IsNormalAvailable() ? model.Mesh.Normals : model.Mesh.ComputeNormals();
            auto const &          texcoords = model.Mesh.IsTexCoordAvailable() ? model.Mesh.TexCoords : model.Mesh.GetEmptyTexCoords();
            std::uint32_t const * face      = model.Mesh.Indices.data() + hitMeshIdx;
            glm::vec3 const &     p1        = model.Mesh.Positions[face[0]];
            glm::vec3 const &     p2        = model.Mesh.Positions[face[1]];
            glm::vec3 const &     p3        = model.Mesh.Positions[face[2]];
            glm::vec3 const &     n1        = normals[face[0]];
            glm::vec3 const &     n2        = normals[face[1]];
            glm::vec3 const &     n3        = normals[face[2]];
            glm::vec2 const &     uv1       = texcoords[face[0]];
            glm::vec2 const &     uv2       = texcoords[face[1]];
            glm::vec2 const &     uv3       = texcoords[face[2]];

            result.IntersectState    = true;
            auto const & material    = InternalScene->Materials[model.MaterialIndex];
            result.IntersectMode     = material.Blend;
            result.IntersectPosition = (1.0f - umin - vmin) * p1 + umin * p2 + vmin * p3;
            result.IntersectNormal   = glm::normalize((1.0f - umin - vmin) * n1 + umin * n2 + vmin * n3);
            glm::vec2 uvCoord        = (1.0f - umin - vmin) * uv1 + umin * uv2 + vmin * uv3;
            result.IntersectAlbedo   = GetAlbedo(material, uvCoord);
            result.IntersectMetaSpec = GetTexture(material.MetaSpec, uvCoord);

            return result;
        }
    };

    using RayIntersector = WTXYRayIntersector;

    glm::vec3 RayTrace(const RayIntersector & intersector, Ray ray, int maxDepth, bool enableShadow);

} // namespace VCX::Labs::Rendering
