#include "Labs/MotionMatching/Core/Animation/update.hpp"
#include "Labs/MotionMatching/Core/Animation/bone_operations.hpp"
#include "Labs/MotionMatching/Core/Animation/constant.hpp"
#include "Labs/MotionMatching/Core/Animation/controller.hpp"
#include "Labs/MotionMatching/Core/Math/common.h"
#include "Labs/MotionMatching/Core/Math/quat.h"
#include "Labs/MotionMatching/Core/Math/spring.h"
#include "Labs/MotionMatching/Core/Math/vec.h"
#include <cfloat>

namespace VCX::Labs::MotionMatching::Core::Animation {

    using namespace Math;

    // Helper functions
    static vec3 adjust_character_position(
        const vec3  character_position,
        const vec3  simulation_position,
        const float halflife,
        const float dt) {
        vec3 difference_position = simulation_position - character_position;
        vec3 adjustment_position = damp_adjustment_exact(
            difference_position,
            halflife,
            dt);
        return adjustment_position + character_position;
    }

    static quat adjust_character_rotation(
        const quat  character_rotation,
        const quat  simulation_rotation,
        const float halflife,
        const float dt) {
        quat difference_rotation = quat_abs(quat_normalize(
            quat_mul_inv(simulation_rotation, character_rotation)));

        quat adjustment_rotation = damp_adjustment_exact(
            difference_rotation,
            halflife,
            dt);

        return quat_mul(adjustment_rotation, character_rotation);
    }

    static vec3 clamp_character_position(
        const vec3  character_position,
        const vec3  simulation_position,
        const float max_distance) {
        if (length(character_position - simulation_position) > max_distance) {
            return max_distance * normalize(character_position - simulation_position) + simulation_position;
        } else {
            return character_position;
        }
    }

    static quat clamp_character_rotation(
        const quat  character_rotation,
        const quat  simulation_rotation,
        const float max_angle) {
        if (quat_angle_between(character_rotation, simulation_rotation) > max_angle) {
            quat diff = quat_abs(quat_mul_inv(
                character_rotation, simulation_rotation));

            float diff_angle;
            vec3  diff_axis;
            quat_to_angle_axis(diff, diff_angle, diff_axis);

            diff_angle = clampf(diff_angle, -max_angle, max_angle);

            return quat_mul(
                quat_from_angle_axis(diff_angle, diff_axis), simulation_rotation);
        } else {
            return character_rotation;
        }
    }

    // Helper functions for query vector construction (moved to desired position logic)
    static void query_copy_denormalized_feature(
        slice1d<float>       query,
        int &                offset,
        const int            size,
        const slice1d<float> features,
        const slice1d<float> features_offset,
        const slice1d<float> features_scale) {
        for (int i = 0; i < size; i++) {
            query(offset + i) = features(offset + i) * features_scale(offset + i) + features_offset(offset + i);
        }

        offset += size;
    }

    static void query_compute_trajectory_position_feature(
        slice1d<float>      query,
        int &               offset,
        const vec3          root_position,
        const quat          root_rotation,
        const slice1d<vec3> trajectory_positions) {
        // Compute trajectory positions relative to character root
        for (int i = 1; i < 4; i++) {
            vec3 traj_pos_local = quat_inv_mul_vec3(root_rotation, trajectory_positions(i) - root_position);

            query(offset + 0) = traj_pos_local.x;
            query(offset + 1) = traj_pos_local.z;
            offset += 2;
        }
    }

    static void query_compute_trajectory_direction_feature(
        slice1d<float>      query,
        int &               offset,
        const quat          root_rotation,
        const slice1d<quat> trajectory_rotations) {
        // Compute trajectory directions relative to character root
        for (int i = 1; i < 4; i++) {
            vec3 traj_dir_local = quat_inv_mul_vec3(root_rotation, quat_mul_vec3(trajectory_rotations(i), vec3(0, 0, 1)));

            query(offset + 0) = traj_dir_local.x;
            query(offset + 1) = traj_dir_local.z;
            offset += 2;
        }
    }

    void MotionMatchingUpdate(
        // Simulation State
        vec3 & simulation_position,
        vec3 & simulation_velocity,
        vec3 & simulation_acceleration,
        quat & simulation_rotation,
        vec3 & simulation_angular_velocity,

        // Character State (Visual Root)
        vec3 & character_position,
        quat & character_rotation,
        vec3 & character_velocity,
        vec3 & character_angular_velocity,

        // Trajectory Prediction (Output for visualization/debug)
        slice1d<vec3> trajectory_positions,
        slice1d<vec3> trajectory_velocities,
        slice1d<vec3> trajectory_accelerations,
        slice1d<quat> trajectory_rotations,
        slice1d<vec3> trajectory_angular_velocities,

        // Animation State
        int &   current_frame_index,
        float & current_frame_time,
        float & search_timer,

        // Inertialization State
        slice1d<vec3> bone_offset_positions,
        slice1d<vec3> bone_offset_velocities,
        slice1d<quat> bone_offset_rotations,
        slice1d<vec3> bone_offset_angular_velocities,
        vec3 &        transition_src_position,
        quat &        transition_src_rotation,
        vec3 &        transition_dst_position,
        quat &        transition_dst_rotation,

        // Output Bone Transforms (Global)
        slice1d<vec3> out_bone_positions,
        slice1d<quat> out_bone_rotations,

        // Contact State
        slice1d<bool> contact_states,
        slice1d<bool> contact_locks,
        slice1d<vec3> contact_positions,
        slice1d<vec3> contact_velocities,
        slice1d<vec3> contact_points,
        slice1d<vec3> contact_targets,
        slice1d<vec3> contact_offset_positions,
        slice1d<vec3> contact_offset_velocities,

        // Database & Character
        const database &  db,
        const character & character_data,

        // Input Control
        const vec3 & gamepad_stick_left,
        const vec3 & gamepad_stick_right,
        const float  camera_azimuth,
        const bool   desired_strafe,
        const float  fwrd_speed,
        const float  side_speed,
        const float  back_speed,
        const float  dt,
        const bool   enable_ik) {
        // ----------------------------------------------------------------------------
        // 1. Input Processing & Trajectory Prediction
        // ----------------------------------------------------------------------------

        // Use constants from MotionMatchingConstants
        const float search_time_interval         = MotionMatchingConstants::SearchTimeInterval;
        const float simulation_halflife          = MotionMatchingConstants::SimulationHalflife;
        const float simulation_rotation_halflife = MotionMatchingConstants::SimulationRotationHalflife;
        const float inertialization_halflife     = MotionMatchingConstants::InertializationHalflife;

        vec3 desired_velocity = desired_velocity_update(
            gamepad_stick_left,
            camera_azimuth,
            simulation_rotation,
            fwrd_speed,
            side_speed,
            back_speed);

        quat desired_rotation = desired_rotation_update(
            simulation_rotation,
            gamepad_stick_left,
            gamepad_stick_right,
            camera_azimuth,
            desired_strafe,
            desired_velocity);

        // Check if we should force a search (e.g. if input changed significantly)
        bool force_search = false;
        if (length(gamepad_stick_left) > 0.1f || length(gamepad_stick_right) > 0.1f) {
            // force_search = true; // Maybe too aggressive?
        }

        // Predict Trajectory
        array1d<vec3> desired_velocities(4);
        array1d<quat> desired_rotations(4);

        for (int i = 0; i < 4; ++i) {
            desired_velocities(i)   = desired_velocity;
            desired_rotations(i)    = desired_rotation;
            trajectory_rotations(i) = simulation_rotation;
        }

        trajectory_desired_velocities_predict(
            desired_velocities,
            trajectory_rotations,
            desired_velocity,
            camera_azimuth,
            gamepad_stick_left,
            gamepad_stick_right,
            desired_strafe,
            fwrd_speed,
            side_speed,
            back_speed,
            MotionMatchingConstants::TrajectoryPredictionFactor / 60.0f);

        trajectory_desired_rotations_predict(
            desired_rotations,
            desired_velocities,
            desired_rotation,
            camera_azimuth,
            gamepad_stick_left,
            gamepad_stick_right,
            desired_strafe,
            MotionMatchingConstants::TrajectoryPredictionFactor / 60.0f);

        trajectory_rotations_predict(
            trajectory_rotations,
            trajectory_angular_velocities,
            simulation_rotation,
            simulation_angular_velocity,
            desired_rotations,
            simulation_rotation_halflife,
            MotionMatchingConstants::TrajectoryPredictionFactor / 60.0f);

        trajectory_positions_predict(
            trajectory_positions,
            trajectory_velocities,
            trajectory_accelerations,
            simulation_position,
            simulation_velocity,
            simulation_acceleration,
            desired_velocities,
            simulation_halflife,
            MotionMatchingConstants::TrajectoryPredictionFactor / 60.0f);

        // ----------------------------------------------------------------------------
        // 2. Motion Matching Query & Search
        // ----------------------------------------------------------------------------

        search_timer -= dt;

        // Check if we reached the end of the clip
        int  next_frame_index = current_frame_index + 1;
        bool end_of_range     = true;
        for (int i = 0; i < db.nranges(); ++i) {
            if (next_frame_index >= db.range_starts(i) && next_frame_index < db.range_stops(i)) {
                end_of_range = false;
                break;
            }
        }

        if (search_timer <= 0.0f || force_search || end_of_range) {
            search_timer = search_time_interval;

            // Construct Query Vector using helper functions (moved to desired position logic)
            array1d<float> query(db.nfeatures());
            int            offset = 0;

            // Copy bone features from the current frame in the database
            // Left Foot Position (3 features)
            query_copy_denormalized_feature(query, offset, 3, db.features(current_frame_index), db.features_offset, db.features_scale);
            // Right Foot Position (3 features)
            query_copy_denormalized_feature(query, offset, 3, db.features(current_frame_index), db.features_offset, db.features_scale);
            // Left Foot Velocity (3 features)
            query_copy_denormalized_feature(query, offset, 3, db.features(current_frame_index), db.features_offset, db.features_scale);
            // Right Foot Velocity (3 features)
            query_copy_denormalized_feature(query, offset, 3, db.features(current_frame_index), db.features_offset, db.features_scale);
            // Hip Velocity (3 features)
            query_copy_denormalized_feature(query, offset, 3, db.features(current_frame_index), db.features_offset, db.features_scale);

            // Compute Trajectory Features (Positions) - 6 features (3 time points * 2 coordinates)
            query_compute_trajectory_position_feature(query, offset, character_position, character_rotation, trajectory_positions);

            // Compute Trajectory Features (Directions) - 6 features (3 time points * 2 coordinates)
            query_compute_trajectory_direction_feature(query, offset, character_rotation, trajectory_rotations);

            // Verify we have the correct number of features (15 + 6 + 6 = 27)
            assert(offset == db.nfeatures());

            // Search Database
            int   best_index = -1;
            float best_cost  = FLT_MAX;

            database_search(
                best_index,
                best_cost,
                db,
                query,
                0.0f,
                20,
                20);

            if (best_index != next_frame_index) {
                // Transition
                inertialize_root_adjust(
                    bone_offset_positions(0),
                    transition_src_position,
                    transition_src_rotation,
                    transition_dst_position,
                    transition_dst_rotation,
                    character_position,
                    character_rotation,
                    simulation_position,
                    simulation_rotation);

                inertialize_pose_transition(
                    bone_offset_positions,
                    bone_offset_velocities,
                    bone_offset_rotations,
                    bone_offset_angular_velocities,
                    transition_src_position,
                    transition_src_rotation,
                    transition_dst_position,
                    transition_dst_rotation,
                    character_position,         // root_position
                    character_velocity,         // root_velocity
                    character_rotation,         // root_rotation
                    character_angular_velocity, // root_angular_velocity
                    db.bone_positions(current_frame_index),
                    db.bone_velocities(current_frame_index),
                    db.bone_rotations(current_frame_index),
                    db.bone_angular_velocities(current_frame_index),
                    db.bone_positions(best_index),
                    db.bone_velocities(best_index),
                    db.bone_rotations(best_index),
                    db.bone_angular_velocities(best_index));

                current_frame_index = best_index;
            } else {
                current_frame_index = next_frame_index;
            }
        } else {
            current_frame_index = next_frame_index;
        }

        // ----------------------------------------------------------------------------
        // 3. State Update & Synchronization
        // ----------------------------------------------------------------------------

        // Update Simulation State
        simulation_positions_update(
            simulation_position,
            simulation_velocity,
            simulation_acceleration,
            desired_velocity,
            simulation_halflife,
            dt);

        simulation_rotations_update(
            simulation_rotation,
            simulation_angular_velocity,
            desired_rotation,
            simulation_rotation_halflife,
            dt);

        // SIMPLE SYNCHRONIZATION (synchronization = 1, direct update)
        character_position         = simulation_position;
        character_rotation         = simulation_rotation;
        character_velocity         = simulation_velocity;
        character_angular_velocity = simulation_angular_velocity;

        // ----------------------------------------------------------------------------
        // 4. Pose Extraction & Inertialization
        // ----------------------------------------------------------------------------

        array1d<vec3> current_bone_positions          = db.bone_positions(current_frame_index);
        array1d<vec3> current_bone_velocities         = db.bone_velocities(current_frame_index);
        array1d<quat> current_bone_rotations          = db.bone_rotations(current_frame_index);
        array1d<vec3> current_bone_angular_velocities = db.bone_angular_velocities(current_frame_index);

        array1d<vec3> inertialized_bone_positions(db.nbones());
        array1d<vec3> inertialized_bone_velocities(db.nbones());
        array1d<quat> inertialized_bone_rotations(db.nbones());
        array1d<vec3> inertialized_bone_angular_velocities(db.nbones());

        inertialize_pose_update(
            inertialized_bone_positions,
            inertialized_bone_velocities,
            inertialized_bone_rotations,
            inertialized_bone_angular_velocities,
            bone_offset_positions,
            bone_offset_velocities,
            bone_offset_rotations,
            bone_offset_angular_velocities,
            current_bone_positions,
            current_bone_velocities,
            current_bone_rotations,
            current_bone_angular_velocities,
            transition_src_position,
            transition_src_rotation,
            transition_dst_position,
            transition_dst_rotation,
            inertialization_halflife,
            dt);

        // Update Character Root with Visual State
        inertialized_bone_positions(0)          = character_position;
        inertialized_bone_rotations(0)          = character_rotation;
        inertialized_bone_velocities(0)         = character_velocity;
        inertialized_bone_angular_velocities(0) = character_angular_velocity;

        forward_kinematics_full(
            out_bone_positions,
            out_bone_rotations,
            inertialized_bone_positions,
            inertialized_bone_rotations,
            db.bone_parents);

        // ----------------------------------------------------------------------------
        // 5. Post-Processing (IK)
        // ----------------------------------------------------------------------------

        if (enable_ik) {
            array1d<int> contact_bones(2);
            contact_bones(0) = Bone_LeftToe;
            contact_bones(1) = Bone_RightToe;

            // Use IK parameters from MotionMatchingConstants
            float ik_unlock_radius     = MotionMatchingConstants::IKUnlockRadius;
            float ik_foot_height       = MotionMatchingConstants::IKFootHeight;
            float ik_blending_halflife = MotionMatchingConstants::IKBlendingHalflife;
            float ik_max_length_buffer = MotionMatchingConstants::IKMaxLengthBuffer;

            array1d<bool> global_bone_computed(db.nbones());

            for (int i = 0; i < contact_bones.size; i++) {
                int toe_bone  = contact_bones(i);
                int heel_bone = db.bone_parents(toe_bone);
                int knee_bone = db.bone_parents(heel_bone);
                int hip_bone  = db.bone_parents(knee_bone);
                int root_bone = db.bone_parents(hip_bone);

                global_bone_computed.zero();

                forward_kinematics_partial(
                    out_bone_positions,
                    out_bone_rotations,
                    global_bone_computed,
                    inertialized_bone_positions,
                    inertialized_bone_rotations,
                    db.bone_parents,
                    toe_bone);

                contact_update(
                    contact_states(i),
                    contact_locks(i),
                    contact_positions(i),
                    contact_velocities(i),
                    contact_points(i),
                    contact_targets(i),
                    contact_offset_positions(i),
                    contact_offset_velocities(i),
                    out_bone_positions(toe_bone),
                    db.contact_states(current_frame_index, i),
                    ik_unlock_radius,
                    ik_foot_height,
                    ik_blending_halflife,
                    dt);

                vec3 contact_position_clamp = contact_positions(i);
                contact_position_clamp.y    = maxf(contact_position_clamp.y, ik_foot_height);

                for (int bone : { heel_bone, knee_bone, hip_bone, root_bone }) {
                    forward_kinematics_partial(
                        out_bone_positions,
                        out_bone_rotations,
                        global_bone_computed,
                        inertialized_bone_positions,
                        inertialized_bone_rotations,
                        db.bone_parents,
                        bone);
                }

                ik_two_bone(
                    inertialized_bone_rotations(hip_bone),
                    inertialized_bone_rotations(knee_bone),
                    out_bone_positions(hip_bone),
                    out_bone_positions(knee_bone),
                    out_bone_positions(heel_bone),
                    contact_position_clamp + (out_bone_positions(heel_bone) - out_bone_positions(toe_bone)),
                    quat_mul_vec3(out_bone_rotations(knee_bone), vec3(0.0f, 1.0f, 0.0f)),
                    out_bone_rotations(hip_bone),
                    out_bone_rotations(knee_bone),
                    out_bone_rotations(root_bone),
                    ik_max_length_buffer);

                global_bone_computed.zero();

                for (int bone : { toe_bone, heel_bone, knee_bone }) {
                    forward_kinematics_partial(
                        out_bone_positions,
                        out_bone_rotations,
                        global_bone_computed,
                        inertialized_bone_positions,
                        inertialized_bone_rotations,
                        db.bone_parents,
                        bone);
                }

                ik_look_at(
                    inertialized_bone_rotations(heel_bone),
                    out_bone_rotations(knee_bone),
                    out_bone_rotations(heel_bone),
                    out_bone_positions(heel_bone),
                    out_bone_positions(toe_bone),
                    contact_position_clamp);
            }

            forward_kinematics_full(
                out_bone_positions,
                out_bone_rotations,
                inertialized_bone_positions,
                inertialized_bone_rotations,
                db.bone_parents);
        }
    }

} // namespace VCX::Labs::MotionMatching::Core::Animation
