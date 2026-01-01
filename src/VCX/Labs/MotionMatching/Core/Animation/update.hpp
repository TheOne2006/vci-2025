#pragma once

#include "../Math/array.h"
#include "../Math/quat.h"
#include "../Math/vec.h"
#include "character.hpp"
#include "database.hpp"

namespace VCX::Labs::MotionMatching::Core::Animation {

    using namespace Math;

    /**
     * @brief Updates the Motion Matching system for a single frame.
     *
     * This function handles the entire pipeline:
     * 1. Trajectory Prediction based on input.
     * 2. Motion Matching Search to find the best animation frame.
     * 3. State Update & Synchronization with the simulation object.
     * 4. Pose Extraction & Inertialization for smooth transitions.
     * 5. Post-Processing (IK) for foot locking.
     *
     * @param simulation_position [In/Out] Current position of the simulation object (capsule).
     * @param simulation_velocity [In/Out] Current velocity of the simulation object.
     * @param simulation_acceleration [In/Out] Current acceleration of the simulation object.
     * @param simulation_rotation [In/Out] Current rotation of the simulation object.
     * @param simulation_angular_velocity [In/Out] Current angular velocity of the simulation object.
     * 
     * @param character_position [In/Out] Current position of the visual character (root).
     * @param character_rotation [In/Out] Current rotation of the visual character (root).
     * @param character_velocity [In/Out] Current velocity of the visual character (root).
     * @param character_angular_velocity [In/Out] Current angular velocity of the visual character (root).
     *
     * @param trajectory_positions [Out] Predicted future positions (size: 4).
     * @param trajectory_velocities [Out] Predicted future velocities (size: 4).
     * @param trajectory_accelerations [Out] Predicted future accelerations (size: 4).
     * @param trajectory_rotations [Out] Predicted future rotations (size: 4).
     * @param trajectory_angular_velocities [Out] Predicted future angular velocities (size: 4).
     *
     * @param current_frame_index [In/Out] Index of the current animation frame in the database.
     * @param current_frame_time [In/Out] Accumulated time for the current frame.
     * @param search_timer [In/Out] Timer for periodic searching.
     *
     * @param bone_offset_positions [In/Out] Inertialization offsets for bone positions.
     * @param bone_offset_velocities [In/Out] Inertialization offsets for bone velocities.
     * @param bone_offset_rotations [In/Out] Inertialization offsets for bone rotations.
     * @param bone_offset_angular_velocities [In/Out] Inertialization offsets for bone angular velocities.
     * @param transition_src_position [In/Out] Source position for root transition inertialization.
     * @param transition_src_rotation [In/Out] Source rotation for root transition inertialization.
     * @param transition_dst_position [In/Out] Destination position for root transition inertialization.
     * @param transition_dst_rotation [In/Out] Destination rotation for root transition inertialization.
     *
     * @param out_bone_positions [Out] Final global bone positions for rendering.
     * @param out_bone_rotations [Out] Final global bone rotations for rendering.
     *
     * @param contact_states [In/Out] Contact states for feet.
     * @param contact_locks [In/Out] Contact locks for feet.
     * @param contact_positions [In/Out] Contact positions for feet.
     * @param contact_velocities [In/Out] Contact velocities for feet.
     * @param contact_points [In/Out] Contact points for feet.
     * @param contact_targets [In/Out] Contact targets for feet.
     * @param contact_offset_positions [In/Out] Contact offset positions for feet.
     * @param contact_offset_velocities [In/Out] Contact offset velocities for feet.
     *
     * @param db [In] The motion matching database.
     * @param character_data [In] The character skeleton data.
     *
     * @param gamepad_stick_left [In] Left stick input (movement).
     * @param gamepad_stick_right [In] Right stick input (camera/facing).
     * @param camera_azimuth [In] Camera azimuth angle in radians.
     * @param desired_strafe [In] Whether strafing is enabled.
     * @param dt [In] Delta time for this frame.
     * @param enable_ik [In] Whether to enable foot locking IK.
     */
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
        const float  dt,
        const bool   enable_ik);

} // namespace VCX::Labs::MotionMatching::Core::Animation
