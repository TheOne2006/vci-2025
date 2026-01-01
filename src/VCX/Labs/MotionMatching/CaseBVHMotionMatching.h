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

    class CaseBVHMotionMatching : public Common::ICase {
    public:
        CaseBVHMotionMatching();

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
        Core::Math::array1d<Core::Math::vec3> _bone_positions;
        Core::Math::array1d<Core::Math::vec3> _bone_velocities;
        Core::Math::array1d<Core::Math::quat> _bone_rotations;
        Core::Math::array1d<Core::Math::vec3> _bone_angular_velocities;

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
    };
} // namespace VCX::Labs::MotionMatching
