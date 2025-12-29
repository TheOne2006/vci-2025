#pragma once

#include <memory>

#include "Labs/Common/ICase.h"

// 包含 VCX 框架头文件
#include "Engine/Camera.hpp"
#include "Engine/GL/Frame.hpp"
#include "Engine/GL/Program.h"
#include "Engine/GL/RenderItem.h"
#include "Labs/Common/OrbitCameraManager.h"

// 包含核心模块头文件
#include "Core/Math/quat.h"
#include "Core/Math/vec.h"

// 前向声明核心模块类型
namespace VCX::Labs::MotionMatching::Core::Animation {
    struct character;
}

namespace VCX::Labs::MotionMatching::Core::MotionMatching {
    struct database;
}

namespace VCX::Labs::MotionMatching::Core::Input {
    struct GamepadController;
    struct CharacterController;
    struct KeyboardMouseController;
}

// 前向声明新的渲染器
namespace VCX::Labs::MotionMatching {
    class MotionMatchingRenderer;
}

namespace VCX::Labs::MotionMatching {

    class CaseMotionMatching : public Common::ICase {
    public:
        CaseMotionMatching();
        ~CaseMotionMatching();

        virtual std::string_view const GetName() override { return "Motion Matching Demo"; }

        virtual void                     OnSetupPropsUI() override;
        virtual Common::CaseRenderResult OnRender(std::pair<std::uint32_t, std::uint32_t> const desiredSize) override;
        virtual void                     OnProcessInput(ImVec2 const & pos) override;

    private:
        // 运动匹配状态
        bool _isInitialized = false;

        // VCX 框架组件
        Engine::GL::UniqueProgram               _program;
        Engine::GL::UniqueProgram               _groundProgram;
        Engine::GL::UniqueRenderFrame           _frame;
        Engine::Camera                          _camera;
        Common::OrbitCameraManager              _cameraManager;
        std::unique_ptr<MotionMatchingRenderer> _renderer;

        // 核心模块实例
        std::unique_ptr<Core::Animation::character>           _character;
        std::unique_ptr<Core::MotionMatching::database>       _database;
        std::unique_ptr<Core::Input::CharacterController>     _characterController;
        std::unique_ptr<Core::Input::GamepadController>       _gamepadController;
        std::unique_ptr<Core::Input::KeyboardMouseController> _keyboardMouseController;

        // 运动匹配状态机
        struct MotionMatchingState {
            // 当前动画状态
            int   current_frame  = 0;
            int   best_frame     = 0;
            float blend_time     = 0.0f;
            float blend_duration = 0.1f;

            // 搜索参数
            float            search_threshold    = 0.1f;
            int              search_history_size = 10;
            std::vector<int> search_history;

            // 惯性化状态
            Core::Math::vec3 inertialize_position_offset         = Core::Math::vec3(0.0f, 0.0f, 0.0f);
            Core::Math::vec3 inertialize_velocity_offset         = Core::Math::vec3(0.0f, 0.0f, 0.0f);
            Core::Math::quat inertialize_rotation_offset         = Core::Math::quat();
            Core::Math::vec3 inertialize_angular_velocity_offset = Core::Math::vec3(0.0f, 0.0f, 0.0f);

            // 重置状态
            void reset() {
                current_frame = 0;
                best_frame    = 0;
                blend_time    = 0.0f;
                search_history.clear();
            }
        };

        std::unique_ptr<MotionMatchingState> _motionMatchingState;

        // UI 控制参数
        float _featureWeightFootPosition         = 0.75f;
        float _featureWeightFootVelocity         = 1.0f;
        float _featureWeightHipVelocity          = 1.0f;
        float _featureWeightTrajectoryPositions  = 1.0f;
        float _featureWeightTrajectoryDirections = 1.5f;

        float _simulationVelocityHalflife = 0.27f;
        float _simulationRotationHalflife = 0.27f;

        float _simulationRunFwrdSpeed = 4.0f;
        float _simulationRunSideSpeed = 3.0f;
        float _simulationRunBackSpeed = 2.5f;

        float _simulationWalkFwrdSpeed = 1.75f;
        float _simulationWalkSideSpeed = 1.5f;
        float _simulationWalkBackSpeed = 1.25f;

        float _inertializeBlendingHalflife = 0.1f;

        bool  _synchronizationEnabled    = false;
        float _synchronizationDataFactor = 1.0f;

        bool  _adjustmentEnabled           = true;
        bool  _adjustmentByVelocityEnabled = true;
        float _adjustmentPositionHalflife  = 0.1f;
        float _adjustmentRotationHalflife  = 0.2f;
        float _adjustmentPositionMaxRatio  = 0.5f;
        float _adjustmentRotationMaxRatio  = 0.5f;

        bool  _clampingEnabled     = true;
        float _clampingMaxDistance = 0.15f;
        float _clampingMaxAngle    = 0.5f * 3.1415926535f;

        bool  _ikEnabled          = true;
        float _ikBlendingHalflife = 0.1f;
        float _ikUnlockRadius     = 0.2f;

        // 初始化标志
        void InitializeIfNeeded();
    };
} // namespace VCX::Labs::MotionMatching
