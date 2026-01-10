#pragma once

#include "../Math/array.h"
#include "../Math/common.h"
#include "../Math/quat.h"
#include "../Math/vec.h"
#include "bone_operations.hpp"
#include "character.hpp"

#include <cassert>
#include <cfloat>
#include <cmath>
#include <cstdio>

namespace VCX::Labs::MotionMatching::Core::Animation {

    using namespace Math;

    //--------------------------------------

    enum {
        BOUND_SM_SIZE = 16,
        BOUND_LR_SIZE = 64,
    };

    struct database {
        array2d<vec3> bone_positions;
        array2d<vec3> bone_velocities;
        array2d<quat> bone_rotations;
        array2d<vec3> bone_angular_velocities;
        array1d<int>  bone_parents;

        array1d<int> range_starts;
        array1d<int> range_stops;

        array2d<float> features;
        array1d<float> features_offset;
        array1d<float> features_scale;

        array2d<bool> contact_states;

        array2d<float> bound_sm_min;
        array2d<float> bound_sm_max;
        array2d<float> bound_lr_min;
        array2d<float> bound_lr_max;

        int nframes() const { return bone_positions.rows; }
        int nbones() const { return bone_positions.cols; }
        int nranges() const { return range_starts.size; }
        int nfeatures() const { return features.cols; }
        int ncontacts() const { return contact_states.cols; }
    };

    // Function declarations
    void database_load(database & db, const char * filename);
    void database_save_matching_features(const database & db, const char * filename);
    int  database_trajectory_index_clamp(database & db, int frame, int offset);

    void normalize_feature(
        slice2d<float> features,
        slice1d<float> features_offset,
        slice1d<float> features_scale,
        const int      offset,
        const int      size,
        const float    weight = 1.0f);

    void denormalize_features(
        slice1d<float>       features,
        const slice1d<float> features_offset,
        const slice1d<float> features_scale);

    void compute_bone_position_feature(database & db, int & offset, int bone, float weight = 1.0f);
    void compute_bone_velocity_feature(database & db, int & offset, int bone, float weight = 1.0f);
    void compute_trajectory_position_feature(database & db, int & offset, float weight = 1.0f);
    void compute_trajectory_direction_feature(database & db, int & offset, float weight = 1.0f);
    void database_build_matching_features(
        database &  db,
        const float feature_weight_foot_position,
        const float feature_weight_foot_velocity,
        const float feature_weight_hip_velocity,
        const float feature_weight_trajectory_positions,
        const float feature_weight_trajectory_directions);

    void motion_matching_search(
        int & __restrict__ best_index,
        float & __restrict__ best_cost,
        const slice1d<int>   range_starts,
        const slice1d<int>   range_stops,
        const slice2d<float> features,
        const slice1d<float> features_offset,
        const slice1d<float> features_scale,
        const slice2d<float> bound_sm_min,
        const slice2d<float> bound_sm_max,
        const slice2d<float> bound_lr_min,
        const slice2d<float> bound_lr_max,
        const slice1d<float> query_normalized,
        const float          transition_cost,
        const int            ignore_range_end,
        const int            ignore_surrounding);

    void database_search(
        int &                best_index,
        float &              best_cost,
        const database &     db,
        const slice1d<float> query,
        const float          transition_cost    = 0.0f,
        const int            ignore_range_end   = 20,
        const int            ignore_surrounding = 20);

} // namespace VCX::Labs::MotionMatching::Core::Animation
