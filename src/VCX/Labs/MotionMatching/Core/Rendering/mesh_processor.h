#pragma once

#include "../Animation/character.h"
#include "../Math/array.h"
#include "../Math/common.h"
#include "../Math/quat.h"
#include "../Math/vec.h"

#include <vector>

namespace VCX::Labs::MotionMatching::Core::Rendering {

    using namespace Animation;
    using namespace Math;

    // Simple CPU-based mesh for rendering
    struct SimpleMesh {
        std::vector<vec3>         positions;
        std::vector<vec3>         normals;
        std::vector<vec2>         texcoords;
        std::vector<unsigned int> indices;
    };

    // Create a simple mesh from character data
    inline SimpleMesh create_simple_mesh(const character & c) {
        SimpleMesh mesh;

        // Copy positions
        mesh.positions.resize(c.positions.size);
        for (int i = 0; i < c.positions.size; i++) {
            mesh.positions[i] = c.positions(i);
        }

        // Copy normals
        mesh.normals.resize(c.normals.size);
        for (int i = 0; i < c.normals.size; i++) {
            mesh.normals[i] = c.normals(i);
        }

        // Copy texcoords
        mesh.texcoords.resize(c.texcoords.size);
        for (int i = 0; i < c.texcoords.size; i++) {
            mesh.texcoords[i] = c.texcoords(i);
        }

        // Copy indices
        mesh.indices.resize(c.triangles.size);
        for (int i = 0; i < c.triangles.size; i++) {
            mesh.indices[i] = c.triangles(i);
        }

        return mesh;
    }

    // Apply linear blend skinning to mesh
    inline void apply_skinning(
        SimpleMesh &        mesh,
        const character &   c,
        const slice1d<vec3> bone_anim_positions,
        const slice1d<quat> bone_anim_rotations) {
        // Temporary arrays for skinning
        std::vector<vec3> skinned_positions(c.positions.size);
        std::vector<vec3> skinned_normals(c.normals.size);

        // Apply linear blend skinning to positions
        for (int i = 0; i < c.positions.size; i++) {
            vec3 position = vec3(0.0f, 0.0f, 0.0f);

            for (int j = 0; j < c.bone_indices.cols; j++) {
                if (c.bone_weights(i, j) > 0.0f) {
                    int b = c.bone_indices(i, j);

                    vec3 rest_position = c.positions(i);
                    rest_position      = quat_mul_vec3(quat_inv(c.bone_rest_rotations(b)), rest_position - c.bone_rest_positions(b));
                    rest_position      = quat_mul_vec3(bone_anim_rotations(b), rest_position) + bone_anim_positions(b);

                    position = position + c.bone_weights(i, j) * rest_position;
                }
            }

            skinned_positions[i] = position;
        }

        // Apply linear blend skinning to normals
        for (int i = 0; i < c.normals.size; i++) {
            vec3 normal = vec3(0.0f, 0.0f, 0.0f);

            for (int j = 0; j < c.bone_indices.cols; j++) {
                if (c.bone_weights(i, j) > 0.0f) {
                    int b = c.bone_indices(i, j);

                    vec3 rest_normal = c.normals(i);
                    rest_normal      = quat_mul_vec3(quat_inv(c.bone_rest_rotations(b)), rest_normal);
                    rest_normal      = quat_mul_vec3(bone_anim_rotations(b), rest_normal);

                    normal = normal + c.bone_weights(i, j) * rest_normal;
                }
            }

            skinned_normals[i] = normalize(normal);
        }

        // Update mesh
        mesh.positions = skinned_positions;
        mesh.normals   = skinned_normals;
    }

    // Compute bounding box of mesh
    inline void compute_bounds(
        const SimpleMesh & mesh,
        vec3 &             min_bound,
        vec3 &             max_bound) {
        if (mesh.positions.empty()) {
            min_bound = vec3(0.0f, 0.0f, 0.0f);
            max_bound = vec3(0.0f, 0.0f, 0.0f);
            return;
        }

        min_bound = mesh.positions[0];
        max_bound = mesh.positions[0];

        for (const auto & pos : mesh.positions) {
            min_bound.x = minf(min_bound.x, pos.x);
            min_bound.y = minf(min_bound.y, pos.y);
            min_bound.z = minf(min_bound.z, pos.z);

            max_bound.x = maxf(max_bound.x, pos.x);
            max_bound.y = maxf(max_bound.y, pos.y);
            max_bound.z = maxf(max_bound.z, pos.z);
        }
    }

    // Compute mesh center
    inline vec3 compute_center(const SimpleMesh & mesh) {
        if (mesh.positions.empty()) {
            return vec3(0.0f, 0.0f, 0.0f);
        }

        vec3 sum(0.0f, 0.0f, 0.0f);
        for (const auto & pos : mesh.positions) {
            sum = sum + pos;
        }

        return sum / static_cast<float>(mesh.positions.size());
    }

} // namespace VCX::Labs::MotionMatching::Core::Rendering
