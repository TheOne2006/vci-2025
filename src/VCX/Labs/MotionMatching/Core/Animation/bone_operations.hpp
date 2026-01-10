#pragma once

#include "../Math/array.h"
#include "../Math/common.h"
#include "../Math/quat.h"
#include "../Math/spring.h"
#include "../Math/vec.h"
#include "character.hpp"

#include <cassert>
#include <cmath>

namespace VCX::Labs::MotionMatching::Core::Animation {

    using namespace Math;

    // ============================================================================
    // Forward Kinematics Functions
    // ============================================================================

    // Simple recursive forward kinematics
    void forward_kinematics(
        vec3 &              bone_position,
        quat &              bone_rotation,
        const slice1d<vec3> bone_positions,
        const slice1d<quat> bone_rotations,
        const slice1d<int>  bone_parents,
        const int           bone);

    // Forward kinematics with velocity
    void forward_kinematics_velocity(
        vec3 &              bone_position,
        vec3 &              bone_velocity,
        quat &              bone_rotation,
        vec3 &              bone_angular_velocity,
        const slice1d<vec3> bone_positions,
        const slice1d<vec3> bone_velocities,
        const slice1d<quat> bone_rotations,
        const slice1d<vec3> bone_angular_velocities,
        const slice1d<int>  bone_parents,
        const int           bone);

    // Compute forward kinematics for all joints
    void forward_kinematics_full(
        slice1d<vec3>       global_bone_positions,
        slice1d<quat>       global_bone_rotations,
        const slice1d<vec3> local_bone_positions,
        const slice1d<quat> local_bone_rotations,
        const slice1d<int>  bone_parents);

    // Compute forward kinematics of just some joints using a mask
    void forward_kinematics_partial(
        slice1d<vec3>       global_bone_positions,
        slice1d<quat>       global_bone_rotations,
        slice1d<bool>       global_bone_computed,
        const slice1d<vec3> local_bone_positions,
        const slice1d<quat> local_bone_rotations,
        const slice1d<int>  bone_parents,
        int                 bone);

    // Same but including velocity
    void forward_kinematics_velocity_partial(
        slice1d<vec3>       global_bone_positions,
        slice1d<vec3>       global_bone_velocities,
        slice1d<quat>       global_bone_rotations,
        slice1d<vec3>       global_bone_angular_velocities,
        slice1d<bool>       global_bone_computed,
        const slice1d<vec3> local_bone_positions,
        const slice1d<vec3> local_bone_velocities,
        const slice1d<quat> local_bone_rotations,
        const slice1d<vec3> local_bone_angular_velocities,
        const slice1d<int>  bone_parents,
        int                 bone);

    // ============================================================================
    // Inverse Kinematics Functions
    // ============================================================================

    // Rotate joint to look at target position
    void ik_look_at(
        quat &      bone_rotation,
        const quat  global_parent_rotation,
        const quat  global_rotation,
        const vec3  global_position,
        const vec3  child_position,
        const vec3  target_position,
        const float eps = 1e-5f);

    // Basic two-joint IK
    void ik_two_bone(
        quat &      bone_root_lr,
        quat &      bone_mid_lr,
        const vec3  bone_root,
        const vec3  bone_mid,
        const vec3  bone_end,
        const vec3  target,
        const vec3  fwd,
        const quat  bone_root_gr,
        const quat  bone_mid_gr,
        const quat  bone_par_gr,
        const float max_length_buffer);

    // ============================================================================
    // Contact and Foot Locking Functions
    // ============================================================================

    void contact_reset(
        bool &     contact_state,
        bool &     contact_lock,
        vec3 &     contact_position,
        vec3 &     contact_velocity,
        vec3 &     contact_point,
        vec3 &     contact_target,
        vec3 &     contact_offset_position,
        vec3 &     contact_offset_velocity,
        const vec3 input_contact_position,
        const vec3 input_contact_velocity,
        const bool input_contact_state);

    void contact_update(
        bool &      contact_state,
        bool &      contact_lock,
        vec3 &      contact_position,
        vec3 &      contact_velocity,
        vec3 &      contact_point,
        vec3 &      contact_target,
        vec3 &      contact_offset_position,
        vec3 &      contact_offset_velocity,
        const vec3  input_contact_position,
        const bool  input_contact_state,
        const float unlock_radius,
        const float foot_height,
        const float halflife,
        const float dt,
        const float eps = 1e-8f);

    // ============================================================================
    // Inertialization Functions
    // ============================================================================

    void inertialize_root_adjust(
        vec3 &     offset_position,
        vec3 &     transition_src_position,
        quat &     transition_src_rotation,
        vec3 &     transition_dst_position,
        quat &     transition_dst_rotation,
        vec3 &     position,
        quat &     rotation,
        const vec3 input_position,
        const quat input_rotation);

    void inertialize_pose_reset(
        slice1d<vec3> bone_offset_positions,
        slice1d<vec3> bone_offset_velocities,
        slice1d<quat> bone_offset_rotations,
        slice1d<vec3> bone_offset_angular_velocities,
        vec3 &        transition_src_position,
        quat &        transition_src_rotation,
        vec3 &        transition_dst_position,
        quat &        transition_dst_rotation,
        const vec3    root_position,
        const quat    root_rotation);

    void inertialize_pose_transition(
        slice1d<vec3>       bone_offset_positions,
        slice1d<vec3>       bone_offset_velocities,
        slice1d<quat>       bone_offset_rotations,
        slice1d<vec3>       bone_offset_angular_velocities,
        vec3 &              transition_src_position,
        quat &              transition_src_rotation,
        vec3 &              transition_dst_position,
        quat &              transition_dst_rotation,
        const vec3          root_position,
        const vec3          root_velocity,
        const quat          root_rotation,
        const vec3          root_angular_velocity,
        const slice1d<vec3> bone_src_positions,
        const slice1d<vec3> bone_src_velocities,
        const slice1d<quat> bone_src_rotations,
        const slice1d<vec3> bone_src_angular_velocities,
        const slice1d<vec3> bone_dst_positions,
        const slice1d<vec3> bone_dst_velocities,
        const slice1d<quat> bone_dst_rotations,
        const slice1d<vec3> bone_dst_angular_velocities);

    void inertialize_pose_update(
        slice1d<vec3>       bone_positions,
        slice1d<vec3>       bone_velocities,
        slice1d<quat>       bone_rotations,
        slice1d<vec3>       bone_angular_velocities,
        slice1d<vec3>       bone_offset_positions,
        slice1d<vec3>       bone_offset_velocities,
        slice1d<quat>       bone_offset_rotations,
        slice1d<vec3>       bone_offset_angular_velocities,
        const slice1d<vec3> bone_input_positions,
        const slice1d<vec3> bone_input_velocities,
        const slice1d<quat> bone_input_rotations,
        const slice1d<vec3> bone_input_angular_velocities,
        const vec3          transition_src_position,
        const quat          transition_src_rotation,
        const vec3          transition_dst_position,
        const quat          transition_dst_rotation,
        const float         halflife,
        const float         dt);
} // namespace VCX::Labs::MotionMatching::Core::Animation
