#pragma once

#include "bvh11.hpp"
#include "Engine/GL/Frame.hpp"
#include "Engine/GL/Program.h"
#include "Engine/GL/RenderItem.h"
#include "Engine/GL/resource.hpp"
#include "Labs/Common/ICase.h"
#include "Labs/Common/OrbitCameraManager.h"
#include "Labs/MotionMatching/Core/Animation/character.hpp"
#include "Labs/MotionMatching/Core/Math/array.h"
#include "Labs/MotionMatching/Core/Math/quat.h"
#include "Labs/MotionMatching/Core/Math/vec.h"
#include "Labs/MotionMatching/Utils.hpp"
#include "SceneEnvironment.hpp"
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
        void OnBVHLoaded();
        void UpdateFrame(float dt);

    private:
        Engine::GL::UniqueProgram     _program;
        SceneEnvironment              _sceneEnv;
        Engine::GL::UniqueRenderFrame _frame;
        Engine::Camera                _camera { .Eye = glm::vec3(-3, 3, 3) };
        Common::OrbitCameraManager    _cameraManager;
        bool                          _stopped { false };
        bool                          _enableMSAA { true };
        bool                          _showAxis { true };

        // Character Mesh Resources
        Core::Animation::character           _character;
        Engine::GL::UniqueVertexArray        _vao;
        Engine::GL::UniqueArrayBuffer        _vboPos;
        Engine::GL::UniqueArrayBuffer        _vboNorm;
        Engine::GL::UniqueArrayBuffer        _vboTex;
        Engine::GL::UniqueElementArrayBuffer _ebo;

        // BVH Data
        std::unique_ptr<bvh11::BvhObject>                _bvh;
        std::vector<std::shared_ptr<const bvh11::Joint>> _joints;
        std::future<std::unique_ptr<bvh11::BvhObject>>   _loadFuture;

        // Animation Data (using Core::Math types for bone_operations)
        Core::Math::array1d<int>              _boneParents;
        Core::Math::array1d<Core::Math::vec3> _localPositions;
        Core::Math::array1d<Core::Math::quat> _localRotations;
        Core::Math::array1d<Core::Math::vec3> _globalPositions;
        Core::Math::array1d<Core::Math::quat> _globalRotations;

        // Skinning Data (Retargeted)
        Core::Math::array1d<Core::Math::vec3> _skinningPositions;
        Core::Math::array1d<Core::Math::quat> _skinningRotations;
        Core::Math::array1d<Core::Math::vec3> _restPositions;
        Core::Math::array1d<Core::Math::vec3> _restNormals;

        float _currentTime = 0.0f;
        int   _currentBVH  = 0;
        int   _frameIndex  = 0;
    };
} // namespace VCX::Labs::MotionMatching
