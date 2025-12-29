#include "Labs/MotionMatching/CaseMotionMatching.h"
#include "Labs/Common/ImGuiHelper.h"

// 包含核心模块
#include "Core/Animation/character.h"
#include "Core/Input/controller.h"
#include "Core/IO/loader.h"
#include "Core/MotionMatching/database.h"
#include "Core/Physics/spring.h"
#include "Core/Rendering/mesh_processor.h"

// 包含 VCX 框架组件
#include "Engine/Camera.hpp"
#include "Engine/GL/Frame.hpp"
#include "Engine/GL/Program.h"
#include "Engine/GL/RenderItem.h"
#include "Labs/Common/OrbitCameraManager.h"

// 包含新的渲染器
#include "Rendering/MotionMatchingRenderer.h"

#include <chrono>

namespace VCX::Labs::MotionMatching {

    using namespace Core::Animation;
    using namespace Core::MotionMatching;
    using namespace Core::Physics;
    using namespace Core::Input;
    using namespace Core::Rendering;
    using namespace Core::IO;
    using namespace Engine;
    using namespace Engine::GL;

    CaseMotionMatching::CaseMotionMatching():
        _program(
            UniqueProgram({ SharedShader("assets/motion-matching/shaders/character.vert"), SharedShader("assets/motion-matching/shaders/character.frag") })),
        _groundProgram(
            UniqueProgram({ SharedShader("assets/motion-matching/shaders/checkerboard.vert"), SharedShader("assets/motion-matching/shaders/checkerboard.frag") })),
        _camera({ .Eye = glm::vec3(-3, 3, 3) }),
        _renderer(std::make_unique<MotionMatchingRenderer>()) {
        // 构造函数初始化
        _character           = std::make_unique<Core::Animation::character>();
        _database            = std::make_unique<Core::MotionMatching::database>();
        _characterController = std::make_unique<CharacterController>();
        _gamepadController   = std::make_unique<GamepadController>();

        // 初始化运动匹配状态
        _motionMatchingState = std::make_unique<MotionMatchingState>();

        // 初始化相机管理器
        _cameraManager.AutoRotate = false;
        _cameraManager.Save(_camera);
    }

    CaseMotionMatching::~CaseMotionMatching() = default;

    void CaseMotionMatching::InitializeIfNeeded() {
        if (_isInitialized) return;

        // 初始化运动匹配系统
        // 1. 加载角色数据 (character.bin)
        std::string characterPath = "assets/motion-matching/data/character.bin";
        if (! load_character(*_character, characterPath.c_str())) {
            // 如果加载失败，使用默认值
            *_character = character();
        }

        // 2. 加载动画数据库 (database.bin)
        std::string databasePath = "assets/motion-matching/data/database.bin";
        if (! load_database(*_database, databasePath.c_str())) {
            // 如果加载失败，使用默认值
            *_database = database();
        }

        // 3. 加载特征数据 (features.bin)
        std::string featuresPath = "assets/motion-matching/data/features.bin";
        if (! load_matching_features(*_database, featuresPath.c_str())) {
            // 如果加载失败，构建特征
            database_build_matching_features(
                *_database,
                _featureWeightFootPosition,
                _featureWeightFootVelocity,
                _featureWeightHipVelocity,
                _featureWeightTrajectoryPositions,
                _featureWeightTrajectoryDirections);
        }

        // 4. 初始化渲染器
        if (_renderer->Initialize()) {
            _renderer->SetCharacterData(*_character, *_database);
        }

        // 5. 初始化键盘鼠标控制器
        _keyboardMouseController = std::make_unique<KeyboardMouseController>();

        _isInitialized = true;
    }

    void CaseMotionMatching::OnSetupPropsUI() {
        InitializeIfNeeded();

        ImGui::Text("Motion Matching Demo");
        ImGui::Separator();

        if (ImGui::CollapsingHeader("Feature Weights", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::SliderFloat("Foot Position", &_featureWeightFootPosition, 0.0f, 2.0f, "%.2f");
            ImGui::SliderFloat("Foot Velocity", &_featureWeightFootVelocity, 0.0f, 2.0f, "%.2f");
            ImGui::SliderFloat("Hip Velocity", &_featureWeightHipVelocity, 0.0f, 2.0f, "%.2f");
            ImGui::SliderFloat("Trajectory Positions", &_featureWeightTrajectoryPositions, 0.0f, 2.0f, "%.2f");
            ImGui::SliderFloat("Trajectory Directions", &_featureWeightTrajectoryDirections, 0.0f, 2.0f, "%.2f");
        }

        if (ImGui::CollapsingHeader("Simulation Parameters")) {
            ImGui::SliderFloat("Velocity Halflife", &_simulationVelocityHalflife, 0.01f, 1.0f, "%.2f");
            ImGui::SliderFloat("Rotation Halflife", &_simulationRotationHalflife, 0.01f, 1.0f, "%.2f");

            ImGui::SeparatorText("Run Speeds");
            ImGui::SliderFloat("Forward", &_simulationRunFwrdSpeed, 0.0f, 10.0f, "%.2f");
            ImGui::SliderFloat("Side", &_simulationRunSideSpeed, 0.0f, 10.0f, "%.2f");
            ImGui::SliderFloat("Back", &_simulationRunBackSpeed, 0.0f, 10.0f, "%.2f");

            ImGui::SeparatorText("Walk Speeds");
            ImGui::SliderFloat("Forward##walk", &_simulationWalkFwrdSpeed, 0.0f, 10.0f, "%.2f");
            ImGui::SliderFloat("Side##walk", &_simulationWalkSideSpeed, 0.0f, 10.0f, "%.2f");
            ImGui::SliderFloat("Back##walk", &_simulationWalkBackSpeed, 0.0f, 10.0f, "%.2f");
        }

        if (ImGui::CollapsingHeader("Blending & Adjustment")) {
            ImGui::SliderFloat("Inertialize Halflife", &_inertializeBlendingHalflife, 0.01f, 1.0f, "%.2f");

            ImGui::Checkbox("Synchronization", &_synchronizationEnabled);
            if (_synchronizationEnabled) {
                ImGui::SliderFloat("Sync Data Factor", &_synchronizationDataFactor, 0.0f, 2.0f, "%.2f");
            }

            ImGui::Checkbox("Adjustment", &_adjustmentEnabled);
            if (_adjustmentEnabled) {
                ImGui::Checkbox("By Velocity", &_adjustmentByVelocityEnabled);
                ImGui::SliderFloat("Pos Halflife", &_adjustmentPositionHalflife, 0.01f, 1.0f, "%.2f");
                ImGui::SliderFloat("Rot Halflife", &_adjustmentRotationHalflife, 0.01f, 1.0f, "%.2f");
                ImGui::SliderFloat("Pos Max Ratio", &_adjustmentPositionMaxRatio, 0.0f, 1.0f, "%.2f");
                ImGui::SliderFloat("Rot Max Ratio", &_adjustmentRotationMaxRatio, 0.0f, 1.0f, "%.2f");
            }

            ImGui::Checkbox("Clamping", &_clampingEnabled);
            if (_clampingEnabled) {
                ImGui::SliderFloat("Max Distance", &_clampingMaxDistance, 0.0f, 1.0f, "%.2f");
                ImGui::SliderFloat("Max Angle", &_clampingMaxAngle, 0.0f, 3.1415926535f, "%.2f");
            }
        }

        if (ImGui::CollapsingHeader("Inverse Kinematics")) {
            ImGui::Checkbox("IK Enabled", &_ikEnabled);
            if (_ikEnabled) {
                ImGui::SliderFloat("IK Blending Halflife", &_ikBlendingHalflife, 0.01f, 1.0f, "%.2f");
                ImGui::SliderFloat("IK Unlock Radius", &_ikUnlockRadius, 0.0f, 1.0f, "%.2f");
            }
        }

        ImGui::Separator();
        if (ImGui::Button("Reset Parameters")) {
            // 重置所有参数到默认值
            _featureWeightFootPosition         = 0.75f;
            _featureWeightFootVelocity         = 1.0f;
            _featureWeightHipVelocity          = 1.0f;
            _featureWeightTrajectoryPositions  = 1.0f;
            _featureWeightTrajectoryDirections = 1.5f;

            _simulationVelocityHalflife = 0.27f;
            _simulationRotationHalflife = 0.27f;

            _simulationRunFwrdSpeed = 4.0f;
            _simulationRunSideSpeed = 3.0f;
            _simulationRunBackSpeed = 2.5f;

            _simulationWalkFwrdSpeed = 1.75f;
            _simulationWalkSideSpeed = 1.5f;
            _simulationWalkBackSpeed = 1.25f;

            _inertializeBlendingHalflife = 0.1f;

            _synchronizationEnabled    = false;
            _synchronizationDataFactor = 1.0f;

            _adjustmentEnabled           = true;
            _adjustmentByVelocityEnabled = true;
            _adjustmentPositionHalflife  = 0.1f;
            _adjustmentRotationHalflife  = 0.2f;
            _adjustmentPositionMaxRatio  = 0.5f;
            _adjustmentRotationMaxRatio  = 0.5f;

            _clampingEnabled     = true;
            _clampingMaxDistance = 0.15f;
            _clampingMaxAngle    = 0.5f * 3.1415926535f;

            _ikEnabled          = true;
            _ikBlendingHalflife = 0.1f;
            _ikUnlockRadius     = 0.2f;
        }
    }

    Common::CaseRenderResult CaseMotionMatching::OnRender(std::pair<std::uint32_t, std::uint32_t> const desiredSize) {
        InitializeIfNeeded();

        // 调整帧缓冲区大小
        _frame.Resize(desiredSize);

        // 更新相机
        _cameraManager.Update(_camera);

        // 使用帧缓冲区
        gl_using(_frame);

        // 设置光照位置
        glm::vec3 lightPosition = _camera.Eye + glm::vec3(2.0f, 2.0f, 2.0f);

        // 渲染地面
        _renderer->RenderGround(_groundProgram, _camera);

        // 渲染角色
        _renderer->RenderCharacter(_program, _camera, lightPosition);

        return Common::CaseRenderResult {
            .Fixed     = false,
            .Flipped   = true,
            .Image     = _frame.GetColorAttachment(),
            .ImageSize = desiredSize,
        };
    }

    void CaseMotionMatching::OnProcessInput(ImVec2 const & pos) {
        InitializeIfNeeded();

        // 处理相机控制
        _cameraManager.ProcessInput(_camera, pos);

        // TODO: 处理角色控制
        // 1. 键盘/鼠标输入处理
        // 2. 更新角色控制器状态
        // 3. 更新运动匹配状态
    }
} // namespace VCX::Labs::MotionMatching
