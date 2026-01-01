#pragma once

#include <tuple>

namespace VCX::Labs::MotionMatching::Core::Animation {

    /**
     * @brief Static manager for Motion Matching system parameters.
     *
     * This class provides centralized access to all system parameters,
     * allowing easy modification and consistency across the codebase.
     * Note: Some parameters are now mutable and can be adjusted at runtime.
     */
    class MotionMatchingConstants {
    public:
        // System parameters
        static constexpr float SearchTimeInterval = 0.1f;
        static float           SimulationHalflife;
        static float           SimulationRotationHalflife;
        static float           InertializationHalflife;

        // Movement speed parameters (running)
        static float ForwardSpeed;
        static float SideSpeed;
        static float BackwardSpeed;

        // Walking speed parameters
        static float WalkForwardSpeed;
        static float WalkSideSpeed;
        static float WalkBackwardSpeed;

        // Gait transition parameters
        static float GaitChangeHalflife;

        // Inverse Kinematics (IK) parameters
        static constexpr float IKUnlockRadius = 0.2f;
        static constexpr float IKFootHeight   = 0.02f;
        static float           IKBlendingHalflife;
        static constexpr float IKMaxLengthBuffer = 0.015f;
        static constexpr float IKToeLength       = 0.15f;

        // Controller parameters
        static constexpr float Deadzone                   = 0.2f;
        static constexpr float TrajectoryPredictionFactor = 20.0f;
        static float           InputRunningSpeed;
        static float           InputWalkingSpeed;

        // Contact bone indices (defined in character.hpp)
        // Left foot contact bone: Bone_LeftToe (5)
        // Right foot contact bone: Bone_RightToe (9)

        // Simulation object visualization parameters
        static constexpr float SimulationRingRadius   = 0.6f;
        static constexpr float SimulationSphereRadius = 0.05f;
        static constexpr float SimulationArrowLength  = 0.6f;
        static constexpr float TrajectorySphereRadius = 0.05f;
        static constexpr float TrajectoryArrowLength  = 0.6f;
        static constexpr float DesiredDirectionLength = 1.0f;

        /**
         * @brief Get the forward speed based on movement direction.
         *
         * @param isForward Whether moving forward
         * @param isBackward Whether moving backward
         * @param isStrafe Whether strafing
         * @return float Speed value
         */
        static float GetSpeed(bool isForward, bool isBackward, bool isStrafe) {
            if (isForward) return ForwardSpeed;
            if (isBackward) return BackwardSpeed;
            if (isStrafe) return SideSpeed;
            return ForwardSpeed; // Default
        }

        /**
         * @brief Get the appropriate halflife for simulation updates.
         *
         * @param forRotation Whether for rotation (true) or position (false)
         * @return float Halflife value
         */
        static float GetSimulationHalflife(bool forRotation = false) {
            return forRotation ? SimulationRotationHalflife : SimulationHalflife;
        }

        /**
         * @brief Get IK parameters as a tuple.
         *
         * @return std::tuple<float, float, float, float>
         *         (unlock_radius, foot_height, blending_halflife, max_length_buffer)
         */
        static auto GetIKParameters() {
            return std::make_tuple(
                IKUnlockRadius,
                IKFootHeight,
                IKBlendingHalflife,
                IKMaxLengthBuffer);
        }
    };

} // namespace VCX::Labs::MotionMatching::Core::Animation
