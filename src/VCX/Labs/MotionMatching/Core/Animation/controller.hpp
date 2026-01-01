#pragma once

#include "Labs/MotionMatching/Core/Math/array.h"
#include "Labs/MotionMatching/Core/Math/quat.h"
#include "Labs/MotionMatching/Core/Math/spring.h"
#include "Labs/MotionMatching/Core/Math/vec.h"

namespace VCX::Labs::MotionMatching::Core::Animation {

    using namespace Math;

    void simulation_positions_update(
        vec3 &      position,
        vec3 &      velocity,
        vec3 &      acceleration,
        const vec3  desired_velocity,
        const float halflife,
        const float dt);

    void simulation_rotations_update(
        quat &      rotation,
        vec3 &      angular_velocity,
        const quat  desired_rotation,
        const float halflife,
        const float dt);

    void trajectory_positions_predict(
        slice1d<vec3>       positions,
        slice1d<vec3>       velocities,
        slice1d<vec3>       accelerations,
        const vec3          position,
        const vec3          velocity,
        const vec3          acceleration,
        const slice1d<vec3> desired_velocities,
        const float         halflife,
        const float         dt);

    void trajectory_rotations_predict(
        slice1d<quat>       rotations,
        slice1d<vec3>       angular_velocities,
        const quat          rotation,
        const vec3          angular_velocity,
        const slice1d<quat> desired_rotations,
        const float         halflife,
        const float         dt);

    vec3 gamepad_get_stick(float gamepadx, float gamepady, const float deadzone = 0.2f);

    vec3 desired_velocity_update(
        const vec3  gamepadstick_left,
        const float camera_azimuth,
        const quat  simulation_rotation,
        const float fwrd_speed,
        const float side_speed,
        const float back_speed);

    quat desired_rotation_update(
        const quat  desired_rotation,
        const vec3  gamepadstick_left,
        const vec3  gamepadstick_right,
        const float camera_azimuth,
        const bool  desired_strafe,
        const vec3  desired_velocity);

    void trajectory_desired_velocities_predict(
        slice1d<vec3>       desired_velocities,
        const slice1d<quat> trajectory_rotations,
        const vec3          desired_velocity,
        const float         camera_azimuth,
        const vec3          gamepadstick_left,
        const vec3          gamepadstick_right,
        const bool          desired_strafe,
        const float         fwrd_speed,
        const float         side_speed,
        const float         back_speed,
        const float         dt);

    void trajectory_desired_rotations_predict(
        slice1d<quat>       desired_rotations,
        const slice1d<vec3> desired_velocities,
        const quat          desired_rotation,
        const float         camera_azimuth,
        const vec3          gamepadstick_left,
        const vec3          gamepadstick_right,
        const bool          desired_strafe,
        const float         dt);

} // namespace VCX::Labs::MotionMatching::Core::Animation
