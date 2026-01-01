#pragma once

#include "bvh11.hpp"
#include "Engine/GL/Frame.hpp"
#include "Engine/GL/Program.h"
#include "Engine/GL/RenderItem.h"
#include "Labs/Common/ICase.h"
#include "Labs/Common/OrbitCameraManager.h"
#include "Labs/MotionMatching/Core/Math/array.h"
#include "Labs/MotionMatching/Core/Math/quat.h"
#include "Labs/MotionMatching/Core/Math/vec.h"
#include <future>

namespace VCX::Labs::MotionMatching {

    class BoxRenderer {
    public:
        BoxRenderer();

        void render(Engine::GL::UniqueProgram & program);
        void calc_vert_position();

        std::vector<glm::vec3> VertsPosition;
        glm::vec3              CenterPosition;
        glm::vec3              MainAxis;
        float                  width  = 0.05f;
        float                  length = 0.2f;

        Engine::GL::UniqueIndexedRenderItem BoxItem;  // render the box
        Engine::GL::UniqueIndexedRenderItem LineItem; // render line on box
    };

    class CaseBVH : public Common::ICase {
    public:
        CaseBVH();

        virtual std::string_view const GetName() override { return "BVH Player"; }

        virtual void                     OnSetupPropsUI() override;
        virtual Common::CaseRenderResult OnRender(std::pair<std::uint32_t, std::uint32_t> const desiredSize) override;
        virtual void                     OnProcessInput(ImVec2 const & pos) override;

    private:
        void OnBVHLoaded();
        void UpdateFrame(float dt);

    private:
        Engine::GL::UniqueProgram     _program;
        Engine::GL::UniqueRenderFrame _frame;
        Engine::Camera                _camera { .Eye = glm::vec3(-3, 3, 3) };
        Common::OrbitCameraManager    _cameraManager;
        bool                          _stopped { false };

        std::vector<BoxRenderer> _boneRenderers;

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

        float _currentTime = 0.0f;
        int   _currentBVH  = 0;
        int   _frameIndex  = 0;
    };
} // namespace VCX::Labs::MotionMatching
