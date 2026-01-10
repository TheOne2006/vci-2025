#pragma once

#include "bvh11.hpp"
#include "Engine/GL/Frame.hpp"
#include "Engine/GL/Program.h"
#include "Engine/GL/RenderItem.h"
#include "Engine/GL/resource.hpp"
#include "Labs/Common/ICase.h"
#include "Labs/Common/OrbitCameraManager.h"
#include "Labs/MotionMatching/Core/Animation/character.hpp"
#include "Labs/MotionMatching/Core/Animation/database.hpp"
#include "Labs/MotionMatching/Core/Math/array.h"
#include "Labs/MotionMatching/Core/Math/quat.h"
#include "Labs/MotionMatching/Core/Math/vec.h"
#include "Labs/MotionMatching/Utils.hpp"
#include "SceneEnvironment.hpp"
#include "SimulationObject.hpp"
#include <future>

namespace VCX::Labs::MotionMatching {

    class CaseMotionMatching : public Common::ICase {
    public:
        CaseMotionMatching();

        virtual std::string_view const GetName() override { return "Motion Matching"; }

        virtual void                     OnSetupPropsUI() override;
        virtual Common::CaseRenderResult OnRender(std::pair<std::uint32_t, std::uint32_t> const desiredSize) override;
        virtual void                     OnProcessInput(ImVec2 const & pos) override;

    private:
        void UpdateController(float dt);

    private:
        Engine::GL::UniqueProgram     _program;
        SceneEnvironment              _sceneEnv;
        Engine::GL::UniqueRenderFrame _frame;
        Engine::Camera                _camera { .Eye = glm::vec3(-3, 3, 3) };
        Common::OrbitCameraManager    _cameraManager;
        bool                          _enableMSAA { true };
        bool                          _showAxis { true };

        // Character Mesh Resources
        Core::Animation::character           _character;
        Engine::GL::UniqueVertexArray        _vao;
        Engine::GL::UniqueArrayBuffer        _vboPos;
        Engine::GL::UniqueArrayBuffer        _vboNorm;
        Engine::GL::UniqueArrayBuffer        _vboTex;
        Engine::GL::UniqueElementArrayBuffer _ebo;

        // Animation Data
        Core::Animation::database             _database;
        Core::Math::array1d<Core::Math::vec3> _bone_offset_positions;
        Core::Math::array1d<Core::Math::vec3> _bone_offset_velocities;
        Core::Math::array1d<Core::Math::quat> _bone_offset_rotations;
        Core::Math::array1d<Core::Math::vec3> _bone_offset_angular_velocities;

        Core::Math::array1d<Core::Math::vec3> _global_bone_positions;
        Core::Math::array1d<Core::Math::quat> _global_bone_rotations;

        // Contact State
        Core::Math::array1d<bool>             _contact_states;
        Core::Math::array1d<bool>             _contact_locks;
        Core::Math::array1d<Core::Math::vec3> _contact_positions;
        Core::Math::array1d<Core::Math::vec3> _contact_velocities;
        Core::Math::array1d<Core::Math::vec3> _contact_points;
        Core::Math::array1d<Core::Math::vec3> _contact_targets;
        Core::Math::array1d<Core::Math::vec3> _contact_offset_positions;
        Core::Math::array1d<Core::Math::vec3> _contact_offset_velocities;

        // Current Frame State
        int              _current_frame_index = 0;
        float            _current_frame_time  = 0.0f;
        Core::Math::vec3 _transition_src_position;
        Core::Math::quat _transition_src_rotation;
        Core::Math::vec3 _transition_dst_position;
        Core::Math::quat _transition_dst_rotation;

        Core::Math::array1d<int> _boneParents;

        // Skinning Data (Retargeted)
        Core::Math::array1d<Core::Math::vec3> _restPositions;
        Core::Math::array1d<Core::Math::vec3> _restNormals;

        // Simulation Object & Controller
        SimulationObject       _simulationObject;
        Core::Math::vec3       _position { 0, 0, 0 };
        Core::Math::vec3       _velocity { 0, 0, 0 };
        Core::Math::vec3       _acceleration { 0, 0, 0 };
        Core::Math::quat       _rotation { 1, 0, 0, 0 };
        Core::Math::vec3       _angularVelocity { 0, 0, 0 };
        Core::Math::vec3       _desiredVelocity { 0, 0, 0 };
        std::vector<glm::vec3> _predictedPositions;
        std::vector<glm::quat> _predictedRotations;

        // Character Visual State
        Core::Math::vec3 _character_position { 0, 0, 0 };
        Core::Math::quat _character_rotation { 1, 0, 0, 0 };
        Core::Math::vec3 _character_velocity { 0, 0, 0 };
        Core::Math::vec3 _character_angular_velocity { 0, 0, 0 };

        float _search_timer = 0.0f;

        bool _controlCharacter = true;
        bool _enableIK         = true;

        // Exposed parameters for UI control
        // Half-life parameters
        float _uiSimulationHalflife         = 0.27f;
        float _uiSimulationRotationHalflife = 0.27f;
        float _uiInertializationHalflife    = 0.1f;
        float _uiGaitChangeHalflife         = 0.1f;
        float _uiIKBlendingHalflife         = 0.1f;

        // Speed parameters
        float _uiForwardSpeed      = 4.0f;
        float _uiSideSpeed         = 3.0f;
        float _uiBackwardSpeed     = 2.5f;
        float _uiWalkForwardSpeed  = 1.75f;
        float _uiWalkSideSpeed     = 1.5f;
        float _uiWalkBackwardSpeed = 1.25f;
        float _uiInputRunningSpeed = 1.3f;
        float _uiInputWalkingSpeed = 1.0f;

        // Adjustment & Clamping
        float _uiAdjustmentPositionHalflife = 0.1f;
        float _uiAdjustmentRotationHalflife = 0.2f;
        float _uiClampingMaxDistance        = 0.15f;
        float _uiClampingMaxAngle           = 1.57f;

        // Parameter ranges
        struct ParamRange {
            float Min;
            float Max;
        };

        static constexpr ParamRange HalfLifeRange   = { 0.01f, 1.0f };
        static constexpr ParamRange SpeedRange      = { 0.1f, 10.0f };
        static constexpr ParamRange InputSpeedRange = { 0.1f, 5.0f };
        static constexpr ParamRange WalkSpeedRange  = { 0.1f, 5.0f };
    };
} // namespace VCX::Labs::MotionMatching
