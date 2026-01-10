#include "Labs/MotionMatching/Core/Animation/controller.hpp"

namespace VCX::Labs::MotionMatching::Core::Animation {

    using namespace Math;

    void simulation_positions_update(
        vec3 &      position,
        vec3 &      velocity,
        vec3 &      acceleration,
        const vec3  desired_velocity,
        const float halflife,
        const float dt) {
        float y    = halflife_to_damping(halflife) / 2.0f;
        vec3  j0   = velocity - desired_velocity;
        vec3  j1   = acceleration + j0 * y;
        float eydt = fast_negexpf(y * dt);

        vec3 position_prev = position;

        // 积分位置
        position = eydt * (((-j1) / (y * y)) + ((-j0 - j1 * dt) / y)) + (j1 / (y * y)) + j0 / y + desired_velocity * dt + position_prev;

        // 更新速度和加速度
        velocity     = eydt * (j0 + j1 * dt) + desired_velocity;
        acceleration = eydt * (acceleration - j1 * y * dt);
    }

    void simulation_rotations_update(
        quat &      rotation,
        vec3 &      angular_velocity,
        const quat  desired_rotation,
        const float halflife,
        const float dt) {
        simple_spring_damper_exact(
            rotation,
            angular_velocity,
            desired_rotation,
            halflife,
            dt);
    }

    void trajectory_positions_predict(
        slice1d<vec3>       positions,
        slice1d<vec3>       velocities,
        slice1d<vec3>       accelerations,
        const vec3          position,
        const vec3          velocity,
        const vec3          acceleration,
        const slice1d<vec3> desired_velocities,
        const float         halflife,
        const float         dt) {
        positions(0)     = position;
        velocities(0)    = velocity;
        accelerations(0) = acceleration;

        // 迭代预测未来的每一个时间步
        for (int i = 1; i < positions.size; i++) {
            positions(i)     = positions(i - 1);
            velocities(i)    = velocities(i - 1);
            accelerations(i) = accelerations(i - 1);

            // 复用 simulation_positions_update 进行预测
            simulation_positions_update(
                positions(i),
                velocities(i),
                accelerations(i),
                desired_velocities(i),
                halflife,
                dt);
        }
    }

    void trajectory_rotations_predict(
        slice1d<quat>       rotations,
        slice1d<vec3>       angular_velocities,
        const quat          rotation,
        const vec3          angular_velocity,
        const slice1d<quat> desired_rotations,
        const float         halflife,
        const float         dt) {
        rotations(0)          = rotation;
        angular_velocities(0) = angular_velocity;

        for (int i = 1; i < rotations.size; i++) {
            rotations(i)          = rotations(i - 1);
            angular_velocities(i) = angular_velocities(i - 1);

            simulation_rotations_update(
                rotations(i),
                angular_velocities(i),
                desired_rotations(i),
                halflife,
                dt);
        }
    }

    vec3 gamepad_get_stick(float gamepadx, float gamepady, const float deadzone) {
        float gamepadmag = std::sqrt(gamepadx * gamepadx + gamepady * gamepady);

        // 死区处理与归一化
        if (gamepadmag > deadzone) {
            float gamepaddirx       = gamepadx / gamepadmag;
            float gamepaddiry       = gamepady / gamepadmag;
            float gamepadclippedmag = gamepadmag > 1.0f ? 1.0f : gamepadmag * gamepadmag;
            gamepadx                = gamepaddirx * gamepadclippedmag;
            gamepady                = gamepaddiry * gamepadclippedmag;
        } else {
            gamepadx = 0.0f;
            gamepady = 0.0f;
        }

        return vec3(gamepadx, 0.0f, gamepady);
    }

    vec3 desired_velocity_update(
        const vec3  gamepadstick_left,
        const float camera_azimuth,
        const quat  simulation_rotation,
        const float fwrd_speed,
        const float side_speed,
        const float back_speed) {
        // 1. 将摇杆输入转换到世界空间 (基于相机方位角)
        vec3 global_stick_direction = quat_mul_vec3(
            quat_from_angle_axis(camera_azimuth, vec3(0, 1, 0)), gamepadstick_left);

        // 2. 将世界空间方向转换到角色局部空间 (基于当前角色朝向)
        vec3 local_stick_direction = quat_inv_mul_vec3(
            simulation_rotation, global_stick_direction);

        // 3. 根据前后左右不同的速度限制进行缩放
        vec3 local_desired_velocity = local_stick_direction.z > 0.0 ? vec3(side_speed, 0.0f, fwrd_speed) * local_stick_direction : vec3(side_speed, 0.0f, back_speed) * local_stick_direction;

        // 4. 转换回世界空间
        return quat_mul_vec3(simulation_rotation, local_desired_velocity);
    }

    quat desired_rotation_update(
        const quat  desired_rotation,
        const vec3  gamepadstick_left,
        const vec3  gamepadstick_right,
        const float camera_azimuth,
        const bool  desired_strafe,
        const vec3  desired_velocity) {
        quat desired_rotation_curr = desired_rotation;

        // If strafe is desired then desired rotation is controlled by right stick
        if (desired_strafe) {
            vec3 desired_direction = quat_mul_vec3(quat_from_angle_axis(camera_azimuth, vec3(0, 1, 0)), vec3(0, 0, -1));
            desired_rotation_curr  = quat_from_angle_axis(atan2f(desired_direction.x, desired_direction.z), vec3(0, 1, 0));
        }
        // Otherwise desired rotation is controlled by movement vector
        else if (length(gamepadstick_left) > 0.01f) {
            vec3 desired_direction = normalize(desired_velocity);
            return quat_from_angle_axis(atan2f(desired_direction.x, desired_direction.z), vec3(0, 1, 0));
        }

        return desired_rotation_curr;
    }

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
        const float         dt) {
        desired_velocities.set(desired_velocity);

        for (int i = 0; i < desired_velocities.size; i++) {
            desired_velocities(i) = desired_velocity_update(
                gamepadstick_left,
                camera_azimuth,
                trajectory_rotations(i),
                fwrd_speed,
                side_speed,
                back_speed);
        }
    }

    void trajectory_desired_rotations_predict(
        slice1d<quat>       desired_rotations,
        const slice1d<vec3> desired_velocities,
        const quat          desired_rotation,
        const float         camera_azimuth,
        const vec3          gamepadstick_left,
        const vec3          gamepadstick_right,
        const bool          desired_strafe,
        const float         dt) {
        desired_rotations.set(desired_rotation);

        for (int i = 0; i < desired_rotations.size; i++) {
            desired_rotations(i) = desired_rotation_update(
                desired_rotations(i),
                gamepadstick_left,
                gamepadstick_right,
                camera_azimuth,
                desired_strafe,
                desired_velocities(i));
        }
    }

} // namespace VCX::Labs::MotionMatching::Core::Animation
