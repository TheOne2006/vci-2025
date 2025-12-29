#pragma once

#include "../Math/quat.h"
#include "../Math/vec.h"
#include "../Physics/spring.h"

namespace VCX::Labs::MotionMatching::Core::Input {

    using namespace Math;
    using namespace Physics;

    // Simple gamepad controller simulation
    struct GamepadController {
        // Stick states (normalized to [-1, 1])
        vec3 left_stick  = vec3(0.0f, 0.0f, 0.0f);
        vec3 right_stick = vec3(0.0f, 0.0f, 0.0f);

        // Button states
        bool button_a = false;
        bool button_b = false;
        bool button_x = false;
        bool button_y = false;

        bool left_trigger   = false;
        bool right_trigger  = false;
        bool left_shoulder  = false;
        bool right_shoulder = false;

        // Deadzone for sticks
        float deadzone = 0.2f;

        // Process stick input with deadzone and squaring for better sensitivity
        vec3 process_stick(const vec3 & raw_stick) const {
            float magnitude = length(raw_stick);

            if (magnitude > deadzone) {
                vec3  direction         = raw_stick / magnitude;
                float clipped_magnitude = magnitude > 1.0f ? 1.0f : magnitude * magnitude;
                return direction * clipped_magnitude;
            } else {
                return vec3(0.0f, 0.0f, 0.0f);
            }
        }

        // Update stick states
        void update_sticks(const vec3 & raw_left, const vec3 & raw_right) {
            left_stick  = process_stick(raw_left);
            right_stick = process_stick(raw_right);
        }

        // Get desired velocity based on left stick and camera orientation
        vec3 get_desired_velocity(
            const quat & camera_rotation,
            float        forward_speed,
            float        side_speed,
            float        back_speed) const {
            // Transform stick direction to world space
            vec3 global_stick_direction = quat_mul_vec3(camera_rotation, left_stick);

            // Transform to local space of current facing
            vec3 local_stick_direction = quat_inv_mul_vec3(quat_from_angle_axis(0.0f, vec3(0, 1, 0)), global_stick_direction);

            // Scale by speeds based on forward/backward
            vec3 local_desired_velocity;
            if (local_stick_direction.z > 0.0f) {
                local_desired_velocity = vec3(side_speed, 0.0f, forward_speed) * local_stick_direction;
            } else {
                local_desired_velocity = vec3(side_speed, 0.0f, back_speed) * local_stick_direction;
            }

            // Transform back to world space
            return quat_mul_vec3(quat_from_angle_axis(0.0f, vec3(0, 1, 0)), local_desired_velocity);
        }

        // Get desired rotation based on sticks and strafe mode
        quat get_desired_rotation(
            const quat & current_rotation,
            float        camera_azimuth,
            bool         strafe_mode) const {
            if (strafe_mode) {
                // In strafe mode, use right stick for facing direction
                vec3 desired_direction = quat_mul_vec3(
                    quat_from_angle_axis(camera_azimuth, vec3(0, 1, 0)),
                    vec3(0, 0, -1));

                if (length(right_stick) > 0.01f) {
                    desired_direction = quat_mul_vec3(
                        quat_from_angle_axis(camera_azimuth, vec3(0, 1, 0)),
                        normalize(right_stick));
                }

                return quat_from_angle_axis(atan2f(desired_direction.x, desired_direction.z), vec3(0, 1, 0));
            } else if (length(left_stick) > 0.01f) {
                // In normal mode, use left stick for facing direction
                vec3 desired_direction = normalize(left_stick);
                return quat_from_angle_axis(atan2f(desired_direction.x, desired_direction.z), vec3(0, 1, 0));
            } else {
                // No input, keep current rotation
                return current_rotation;
            }
        }

        // Check if strafe is desired (left trigger)
        bool get_desired_strafe() const {
            return left_trigger;
        }

        // Get gait (walk/run) based on button A
        float get_desired_gait() const {
            return button_a ? 1.0f : 0.0f; // 1.0 = walk, 0.0 = run
        }
    };

    // Keyboard and mouse controller for desktop input
    struct KeyboardMouseController {
        // Movement keys (WASD)
        bool key_w = false;
        bool key_a = false;
        bool key_s = false;
        bool key_d = false;

        // Modifier keys
        bool key_shift = false; // Run/walk toggle
        bool key_ctrl  = false; // Strafe mode

        // Mouse state
        vec2 mouse_delta = vec2(0.0f, 0.0f); // Mouse movement delta
        bool mouse_left  = false;            // Left mouse button
        bool mouse_right = false;            // Right mouse button

        // Sensitivity
        float mouse_sensitivity = 0.01f;
        float key_smoothing     = 0.1f;

        // Internal smoothed movement vector
        vec3 smoothed_movement = vec3(0.0f, 0.0f, 0.0f);

        // Get movement vector from keyboard input
        vec3 get_keyboard_movement() const {
            vec3 movement(0.0f, 0.0f, 0.0f);

            if (key_w) movement.z += 1.0f;
            if (key_s) movement.z -= 1.0f;
            if (key_d) movement.x += 1.0f;
            if (key_a) movement.x -= 1.0f;

            // Normalize diagonal movement
            if (length(movement) > 0.0f) {
                movement = normalize(movement);
            }

            return movement;
        }

        // Update smoothed movement vector
        void update_smoothed_movement(float dt) {
            vec3 target_movement = get_keyboard_movement();
            vec3 dummy_velocity  = vec3(0.0f, 0.0f, 0.0f); // velocity (not used)
            simple_spring_damper_exact(
                smoothed_movement,
                dummy_velocity,
                target_movement,
                key_smoothing,
                dt);
        }

        // Get desired velocity based on keyboard input and camera orientation
        vec3 get_desired_velocity(
            const quat & camera_rotation,
            float        forward_speed,
            float        side_speed,
            float        back_speed) const {
            // Transform movement direction to world space
            vec3 global_movement = quat_mul_vec3(camera_rotation, smoothed_movement);

            // Transform to local space of current facing
            vec3 local_movement = quat_inv_mul_vec3(quat_from_angle_axis(0.0f, vec3(0, 1, 0)), global_movement);

            // Scale by speeds based on forward/backward
            vec3 local_desired_velocity;
            if (local_movement.z > 0.0f) {
                local_desired_velocity = vec3(side_speed, 0.0f, forward_speed) * local_movement;
            } else {
                local_desired_velocity = vec3(side_speed, 0.0f, back_speed) * local_movement;
            }

            // Transform back to world space
            return quat_mul_vec3(quat_from_angle_axis(0.0f, vec3(0, 1, 0)), local_desired_velocity);
        }

        // Get desired rotation based on mouse input
        quat get_desired_rotation(
            const quat & current_rotation,
            float        camera_azimuth,
            bool         strafe_mode) const {
            if (strafe_mode) {
                // In strafe mode, character faces camera forward direction
                vec3 desired_direction = quat_mul_vec3(
                    quat_from_angle_axis(camera_azimuth, vec3(0, 1, 0)),
                    vec3(0, 0, -1));

                return quat_from_angle_axis(atan2f(desired_direction.x, desired_direction.z), vec3(0, 1, 0));
            } else if (length(smoothed_movement) > 0.01f) {
                // In normal mode, character faces movement direction
                vec3 desired_direction = normalize(smoothed_movement);
                return quat_from_angle_axis(atan2f(desired_direction.x, desired_direction.z), vec3(0, 1, 0));
            } else {
                // No input, keep current rotation
                return current_rotation;
            }
        }

        // Check if strafe is desired (Ctrl key)
        bool get_desired_strafe() const {
            return key_ctrl;
        }

        // Get gait (walk/run) based on Shift key
        float get_desired_gait() const {
            return key_shift ? 0.0f : 1.0f; // Shift = run (0.0), no shift = walk (1.0)
        }

        // Update mouse delta (call this from input handler)
        void update_mouse_delta(float dx, float dy) {
            mouse_delta.x = dx * mouse_sensitivity;
            mouse_delta.y = dy * mouse_sensitivity;
        }

        // Reset mouse delta (call after processing)
        void reset_mouse_delta() {
            mouse_delta = vec2(0.0f, 0.0f);
        }
    };

    // Character controller that combines input with simulation
    struct CharacterController {
        // Current state
        vec3 position         = vec3(0.0f, 0.0f, 0.0f);
        vec3 velocity         = vec3(0.0f, 0.0f, 0.0f);
        vec3 acceleration     = vec3(0.0f, 0.0f, 0.0f);
        quat rotation         = quat();
        vec3 angular_velocity = vec3(0.0f, 0.0f, 0.0f);

        // Desired state
        vec3  desired_velocity      = vec3(0.0f, 0.0f, 0.0f);
        quat  desired_rotation      = quat();
        float desired_gait          = 0.0f;
        float desired_gait_velocity = 0.0f;

        // Simulation parameters
        float velocity_halflife = 0.27f;
        float rotation_halflife = 0.27f;
        float gait_halflife     = 0.1f;

        // Speeds (m/s)
        float run_forward_speed  = 4.0f;
        float run_side_speed     = 3.0f;
        float run_back_speed     = 2.5f;
        float walk_forward_speed = 1.75f;
        float walk_side_speed    = 1.5f;
        float walk_back_speed    = 1.25f;

        // Update controller with gamepad input
        void update(
            const GamepadController & gamepad,
            float                     camera_azimuth,
            float                     dt) {
            // Get strafe mode
            bool strafe_mode = gamepad.get_desired_strafe();

            // Update desired gait
            float target_gait = gamepad.get_desired_gait();
            simple_spring_damper_exact(
                desired_gait,
                desired_gait_velocity,
                target_gait,
                gait_halflife,
                dt);

            // Get speeds based on gait
            float forward_speed = lerpf(run_forward_speed, walk_forward_speed, desired_gait);
            float side_speed    = lerpf(run_side_speed, walk_side_speed, desired_gait);
            float back_speed    = lerpf(run_back_speed, walk_back_speed, desired_gait);

            // Update desired velocity
            desired_velocity = gamepad.get_desired_velocity(
                quat_from_angle_axis(camera_azimuth, vec3(0, 1, 0)),
                forward_speed,
                side_speed,
                back_speed);

            // Update desired rotation
            desired_rotation = gamepad.get_desired_rotation(
                rotation,
                camera_azimuth,
                strafe_mode);

            // Update simulation
            simulation_positions_update(
                position,
                velocity,
                acceleration,
                desired_velocity,
                velocity_halflife,
                dt);

            simulation_rotations_update(
                rotation,
                angular_velocity,
                desired_rotation,
                rotation_halflife,
                dt);
        }

    private:
        // Simulation update functions (simplified versions)
        void simulation_positions_update(
            vec3 &       pos,
            vec3 &       vel,
            vec3 &       acc,
            const vec3 & desired_vel,
            float        halflife,
            float        dt) {
            float y    = halflife_to_damping(halflife) / 2.0f;
            vec3  j0   = vel - desired_vel;
            vec3  j1   = acc + j0 * y;
            float eydt = fast_negexpf(y * dt);

            vec3 pos_prev = pos;

            pos = eydt * (((-j1) / (y * y)) + ((-j0 - j1 * dt) / y)) + (j1 / (y * y)) + j0 / y + desired_vel * dt + pos_prev;
            vel = eydt * (j0 + j1 * dt) + desired_vel;
            acc = eydt * (acc - j1 * y * dt);
        }

        void simulation_rotations_update(
            quat &       rot,
            vec3 &       ang_vel,
            const quat & desired_rot,
            float        halflife,
            float        dt) {
            simple_spring_damper_exact(
                rot,
                ang_vel,
                desired_rot,
                halflife,
                dt);
        }
    };

} // namespace VCX::Labs::MotionMatching::Core::Input
