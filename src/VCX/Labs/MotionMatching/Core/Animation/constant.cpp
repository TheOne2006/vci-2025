#include "Labs/MotionMatching/Core/Animation/constant.hpp"

namespace VCX::Labs::MotionMatching::Core::Animation {

    // Initialize static member variables with default values

    // System parameters
    float MotionMatchingConstants::SimulationHalflife         = 0.27f;
    float MotionMatchingConstants::SimulationRotationHalflife = 0.27f;
    float MotionMatchingConstants::InertializationHalflife    = 0.1f;

    // Movement speed parameters (running)
    float MotionMatchingConstants::ForwardSpeed  = 4.0f;
    float MotionMatchingConstants::SideSpeed     = 3.0f;
    float MotionMatchingConstants::BackwardSpeed = 2.5f;

    // Walking speed parameters
    float MotionMatchingConstants::WalkForwardSpeed  = 1.75f;
    float MotionMatchingConstants::WalkSideSpeed     = 1.5f;
    float MotionMatchingConstants::WalkBackwardSpeed = 1.25f;

    // Gait transition parameters
    float MotionMatchingConstants::GaitChangeHalflife = 0.1f;

    // Inverse Kinematics (IK) parameters
    float MotionMatchingConstants::IKBlendingHalflife = 0.1f;

    // Controller parameters
    float MotionMatchingConstants::InputRunningSpeed = 1.3f;
    float MotionMatchingConstants::InputWalkingSpeed = 1.0f;

} // namespace VCX::Labs::MotionMatching::Core::Animation
