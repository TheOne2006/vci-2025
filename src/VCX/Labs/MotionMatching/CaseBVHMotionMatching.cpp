#include "Labs/MotionMatching/CaseBVHMotionMatching.h"
#include "Assets/bundled.h"
#include "Labs/MotionMatching/Core/Animation/character.hpp"
#include "Labs/MotionMatching/Core/Animation/constant.hpp"
#include "Labs/MotionMatching/Core/Animation/controller.hpp"
#include "Labs/MotionMatching/Core/Animation/database.hpp"
#include "Labs/MotionMatching/Core/Animation/update.hpp"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace VCX::Labs::MotionMatching {

    CaseBVHMotionMatching::CaseBVHMotionMatching():
        _program(
            Engine::GL::UniqueProgram({ Engine::GL::SharedShader("assets/shaders/character.vert"), Engine::GL::SharedShader("assets/shaders/character.frag") })) {
        _cameraManager.AutoRotate = false;
        _cameraManager.Save(_camera);

        // Load Character
        Core::Animation::character_load(_character, VCX::Assets::CharacterPath[0].data());

        // Load Database
        Core::Animation::database_load(_database, VCX::Assets::DatabasePath[0].data());

        float feature_weight_foot_position         = 0.75f;
        float feature_weight_foot_velocity         = 1.0f;
        float feature_weight_hip_velocity          = 1.0f;
        float feature_weight_trajectory_positions  = 1.0f;
        float feature_weight_trajectory_directions = 1.5f;

        database_build_matching_features(
            _database,
            feature_weight_foot_position,
            feature_weight_foot_velocity,
            feature_weight_hip_velocity,
            feature_weight_trajectory_positions,
            feature_weight_trajectory_directions);

        // Resize Arrays
        int nbones = _database.nbones();
        _bone_offset_positions.resize(nbones);
        _bone_offset_velocities.resize(nbones);
        _bone_offset_rotations.resize(nbones);
        _bone_offset_angular_velocities.resize(nbones);

        _global_bone_positions.resize(nbones);
        _global_bone_rotations.resize(nbones);

        int ncontacts = _database.ncontacts();
        _contact_states.resize(ncontacts);
        _contact_locks.resize(ncontacts);
        _contact_positions.resize(ncontacts);
        _contact_velocities.resize(ncontacts);
        _contact_points.resize(ncontacts);
        _contact_targets.resize(ncontacts);
        _contact_offset_positions.resize(ncontacts);
        _contact_offset_velocities.resize(ncontacts);

        // Initialize State
        _current_frame_index = _database.range_starts(0);
        _current_frame_time  = 0.0f;

        // Initialize Offsets
        _bone_offset_positions.zero();
        _bone_offset_velocities.zero();
        for (int i = 0; i < nbones; ++i) _bone_offset_rotations(i) = Core::Math::quat(1, 0, 0, 0);
        _bone_offset_angular_velocities.zero();

        _transition_src_position = Core::Math::vec3(0, 0, 0);
        _transition_src_rotation = Core::Math::quat(1, 0, 0, 0);
        _transition_dst_position = Core::Math::vec3(0, 0, 0);
        _transition_dst_rotation = Core::Math::quat(1, 0, 0, 0);

        _boneParents = _database.bone_parents;

        // Store Rest Pose
        _restPositions = _character.positions;
        _restNormals   = _character.normals;

        // Setup Buffers
        glBindBuffer(GL_ARRAY_BUFFER, _vboPos.Get());
        glBufferData(GL_ARRAY_BUFFER, _character.positions.size * sizeof(Core::Math::vec3), _character.positions.data, GL_DYNAMIC_DRAW);

        glBindBuffer(GL_ARRAY_BUFFER, _vboNorm.Get());
        glBufferData(GL_ARRAY_BUFFER, _character.normals.size * sizeof(Core::Math::vec3), _character.normals.data, GL_DYNAMIC_DRAW);

        glBindBuffer(GL_ARRAY_BUFFER, _vboTex.Get());
        glBufferData(GL_ARRAY_BUFFER, _character.texcoords.size * sizeof(Core::Math::vec2), _character.texcoords.data, GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _ebo.Get());
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, _character.triangles.size * sizeof(unsigned short), _character.triangles.data, GL_STATIC_DRAW);

        glBindVertexArray(_vao.Get());

        // Position
        GLint locPos = glGetAttribLocation(_program.Get(), "vertexPosition");
        if (locPos >= 0) {
            glBindBuffer(GL_ARRAY_BUFFER, _vboPos.Get());
            glEnableVertexAttribArray(locPos);
            glVertexAttribPointer(locPos, 3, GL_FLOAT, GL_FALSE, 0, (void *) 0);
        }

        // Normal
        GLint locNorm = glGetAttribLocation(_program.Get(), "vertexNormal");
        if (locNorm >= 0) {
            glBindBuffer(GL_ARRAY_BUFFER, _vboNorm.Get());
            glEnableVertexAttribArray(locNorm);
            glVertexAttribPointer(locNorm, 3, GL_FLOAT, GL_FALSE, 0, (void *) 0);
        }

        // TexCoord
        GLint locTex = glGetAttribLocation(_program.Get(), "vertexTexCoord");
        if (locTex >= 0) {
            glBindBuffer(GL_ARRAY_BUFFER, _vboTex.Get());
            glEnableVertexAttribArray(locTex);
            glVertexAttribPointer(locTex, 2, GL_FLOAT, GL_FALSE, 0, (void *) 0);
        }

        // Color - Set constant white
        GLint locColor = glGetAttribLocation(_program.Get(), "vertexColor");
        if (locColor >= 0) {
            glDisableVertexAttribArray(locColor);
            glVertexAttrib4f(locColor, 1.0f, 1.0f, 1.0f, 1.0f);
        }

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _ebo.Get());
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    void CaseBVHMotionMatching::UpdateController(float dt) {
        using namespace Core::Math;
        using namespace Core::Animation;

        // Apply UI parameters to global constants
        MotionMatchingConstants::SimulationHalflife         = _uiSimulationHalflife;
        MotionMatchingConstants::SimulationRotationHalflife = _uiSimulationRotationHalflife;
        MotionMatchingConstants::InertializationHalflife    = _uiInertializationHalflife;
        MotionMatchingConstants::GaitChangeHalflife         = _uiGaitChangeHalflife;
        MotionMatchingConstants::IKBlendingHalflife         = _uiIKBlendingHalflife;

        MotionMatchingConstants::ForwardSpeed      = _uiForwardSpeed;
        MotionMatchingConstants::SideSpeed         = _uiSideSpeed;
        MotionMatchingConstants::BackwardSpeed     = _uiBackwardSpeed;
        MotionMatchingConstants::WalkForwardSpeed  = _uiWalkForwardSpeed;
        MotionMatchingConstants::WalkSideSpeed     = _uiWalkSideSpeed;
        MotionMatchingConstants::WalkBackwardSpeed = _uiWalkBackwardSpeed;
        MotionMatchingConstants::InputRunningSpeed = _uiInputRunningSpeed;
        MotionMatchingConstants::InputWalkingSpeed = _uiInputWalkingSpeed;

        bool isRunning     = ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift);
        bool desiredStrafe = ImGui::IsKeyDown(ImGuiKey_O) && isRunning;

        // Input
        float x = 0.0f;
        float y = 0.0f;
        if (_controlCharacter) {
            if (ImGui::IsKeyDown(ImGuiKey_W)) y += 1.0f;
            if (ImGui::IsKeyDown(ImGuiKey_S)) y -= 1.0f;
            if (ImGui::IsKeyDown(ImGuiKey_A)) x += 1.0f; // Left
            if (ImGui::IsKeyDown(ImGuiKey_D)) x -= 1.0f; // Right
        }

        float length = std::sqrt(x * x + y * y);
        if (length > 1e-5f) {
            float targetSpeed = isRunning ? MotionMatchingConstants::InputRunningSpeed : MotionMatchingConstants::InputWalkingSpeed;
            float scale       = targetSpeed / length;
            x *= scale;
            y *= scale;
        }

        float fwrdSpeed = isRunning ? MotionMatchingConstants::ForwardSpeed : MotionMatchingConstants::WalkForwardSpeed;
        float sideSpeed = isRunning ? MotionMatchingConstants::SideSpeed : MotionMatchingConstants::WalkSideSpeed;
        float backSpeed = isRunning ? MotionMatchingConstants::BackwardSpeed : MotionMatchingConstants::WalkBackwardSpeed;

        vec3 stickLeft  = gamepad_get_stick(x, y);
        vec3 stickRight = vec3(0, 0, 0); // Right stick not implemented for keyboard yet

        // Get camera azimuth
        // Camera direction is Target - Eye
        glm::vec3 camDir        = glm::normalize(_camera.Target - _camera.Eye);
        float     cameraAzimuth = std::atan2(camDir.x, camDir.z);

        // Compute desired velocity for visualization using MotionMatchingConstants
        _desiredVelocity = desired_velocity_update(
            stickLeft,
            cameraAzimuth,
            _rotation,
            fwrdSpeed,
            sideSpeed,
            backSpeed);

        // Prepare Trajectory Buffers
        int               predictionSteps = 4;
        std::vector<vec3> trajPos(predictionSteps), trajVel(predictionSteps), trajAcc(predictionSteps);
        std::vector<quat> trajRot(predictionSteps);
        std::vector<vec3> trajAngVel(predictionSteps);

        MotionMatchingUpdate(
            _position, _velocity, _acceleration, _rotation, _angularVelocity, _character_position, _character_rotation, _character_velocity, _character_angular_velocity, slice1d<vec3>(predictionSteps, trajPos.data()), slice1d<vec3>(predictionSteps, trajVel.data()), slice1d<vec3>(predictionSteps, trajAcc.data()), slice1d<quat>(predictionSteps, trajRot.data()), slice1d<vec3>(predictionSteps, trajAngVel.data()), _current_frame_index, _current_frame_time, _search_timer, _bone_offset_positions, _bone_offset_velocities, _bone_offset_rotations, _bone_offset_angular_velocities, _transition_src_position, _transition_src_rotation, _transition_dst_position, _transition_dst_rotation, _global_bone_positions, _global_bone_rotations, _contact_states, _contact_locks, _contact_positions, _contact_velocities, _contact_points, _contact_targets, _contact_offset_positions, _contact_offset_velocities, _database, _character, stickLeft, stickRight, cameraAzimuth, desiredStrafe, fwrdSpeed, sideSpeed, backSpeed, dt, _enableIK // enable_ik
        );

        // Update Predicted Positions for Rendering
        _predictedPositions.resize(predictionSteps);
        _predictedRotations.resize(predictionSteps);
        for (int i = 0; i < predictionSteps; ++i) {
            _predictedPositions[i] = glm::vec3(trajPos[i].x, trajPos[i].y, trajPos[i].z);
            _predictedRotations[i] = glm::quat(trajRot[i].w, trajRot[i].x, trajRot[i].y, trajRot[i].z);
        }

        // Deform Mesh
        linear_blend_skinning_positions(
            slice1d<vec3>(_character.positions.size, _character.positions.data),
            slice1d<vec3>(_restPositions.size, _restPositions.data),
            _character.bone_weights,
            _character.bone_indices,
            _character.bone_rest_positions,
            _character.bone_rest_rotations,
            _global_bone_positions,
            _global_bone_rotations);

        linear_blend_skinning_normals(
            slice1d<vec3>(_character.normals.size, _character.normals.data),
            slice1d<vec3>(_restNormals.size, _restNormals.data),
            _character.bone_weights,
            _character.bone_indices,
            _character.bone_rest_rotations,
            _global_bone_rotations);

        // Update GPU Buffers
        glBindBuffer(GL_ARRAY_BUFFER, _vboPos.Get());
        glBufferSubData(GL_ARRAY_BUFFER, 0, _character.positions.size * sizeof(vec3), _character.positions.data);

        glBindBuffer(GL_ARRAY_BUFFER, _vboNorm.Get());
        glBufferSubData(GL_ARRAY_BUFFER, 0, _character.normals.size * sizeof(vec3), _character.normals.data);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    void CaseBVHMotionMatching::OnSetupPropsUI() {
        if (ImGui::Button(_controlCharacter ? "Control: Character" : "Control: Camera")) {
            _controlCharacter = ! _controlCharacter;
        }

        bool isRunning = ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift);
        ImGui::Text(ImGui::IsKeyDown(ImGuiKey_O) && isRunning ? "Strafing Active" : "Hold 'O' to Strafe");
        ImGui::Text(isRunning ? "Running Active" : "Hold 'Shift' to Run");
        ImGui::TextDisabled("(Strafing only available when running)");

        if (ImGui::Button("Reset")) {
            _position                   = Core::Math::vec3(0, 0, 0);
            _velocity                   = Core::Math::vec3(0, 0, 0);
            _acceleration               = Core::Math::vec3(0, 0, 0);
            _rotation                   = Core::Math::quat(1, 0, 0, 0);
            _angularVelocity            = Core::Math::vec3(0, 0, 0);
            _character_position         = Core::Math::vec3(0, 0, 0);
            _character_rotation         = Core::Math::quat(1, 0, 0, 0);
            _character_velocity         = Core::Math::vec3(0, 0, 0);
            _character_angular_velocity = Core::Math::vec3(0, 0, 0);
            _search_timer               = 0.0f;
        }

        ImGui::Checkbox("Anti-aliasing", &_enableMSAA);
        ImGui::SameLine();
        ImGui::Checkbox("Show Axis", &_showAxis);
        ImGui::Checkbox("Enable IK", &_enableIK);

        // Exposed parameters UI
        ImGui::Separator();
        ImGui::Text("Motion Matching Parameters");
        ImGui::TextDisabled("Adjust parameters in real-time");

        // Half-life parameters
        if (ImGui::CollapsingHeader("Half-life Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.6f);

            ImGui::Text("Simulation Half-life (position)");
            ImGui::SliderFloat("##SimulationHalflife", &_uiSimulationHalflife, HalfLifeRange.Min, HalfLifeRange.Max, "%.3f");

            ImGui::Text("Simulation Half-life (rotation)");
            ImGui::SliderFloat("##SimulationRotationHalflife", &_uiSimulationRotationHalflife, HalfLifeRange.Min, HalfLifeRange.Max, "%.3f");

            ImGui::Text("Inertialization Half-life");
            ImGui::SliderFloat("##InertializationHalflife", &_uiInertializationHalflife, HalfLifeRange.Min, HalfLifeRange.Max, "%.3f");

            ImGui::Text("Gait Change Half-life");
            ImGui::SliderFloat("##GaitChangeHalflife", &_uiGaitChangeHalflife, HalfLifeRange.Min, HalfLifeRange.Max, "%.3f");

            ImGui::Text("IK Blending Half-life");
            ImGui::SliderFloat("##IKBlendingHalflife", &_uiIKBlendingHalflife, HalfLifeRange.Min, HalfLifeRange.Max, "%.3f");

            ImGui::PopItemWidth();
        }

        // Speed parameters
        if (ImGui::CollapsingHeader("Speed Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.6f);

            ImGui::Text("Running");
            ImGui::Text("Forward Speed");
            ImGui::SliderFloat("##ForwardSpeedRun", &_uiForwardSpeed, SpeedRange.Min, SpeedRange.Max, "%.2f");
            ImGui::Text("Side Speed");
            ImGui::SliderFloat("##SideSpeedRun", &_uiSideSpeed, SpeedRange.Min, SpeedRange.Max, "%.2f");
            ImGui::Text("Backward Speed");
            ImGui::SliderFloat("##BackwardSpeedRun", &_uiBackwardSpeed, SpeedRange.Min, SpeedRange.Max, "%.2f");

            ImGui::Text("Walking");
            ImGui::Text("Forward Speed");
            ImGui::SliderFloat("##ForwardSpeedWalk", &_uiWalkForwardSpeed, WalkSpeedRange.Min, WalkSpeedRange.Max, "%.2f");
            ImGui::Text("Side Speed");
            ImGui::SliderFloat("##SideSpeedWalk", &_uiWalkSideSpeed, WalkSpeedRange.Min, WalkSpeedRange.Max, "%.2f");
            ImGui::Text("Backward Speed");
            ImGui::SliderFloat("##BackwardSpeedWalk", &_uiWalkBackwardSpeed, WalkSpeedRange.Min, WalkSpeedRange.Max, "%.2f");

            ImGui::Text("Input Sensitivity");
            ImGui::Text("Running Sensitivity");
            ImGui::SliderFloat("##RunningSensitivity", &_uiInputRunningSpeed, InputSpeedRange.Min, InputSpeedRange.Max, "%.2f");
            ImGui::Text("Walking Sensitivity");
            ImGui::SliderFloat("##WalkingSensitivity", &_uiInputWalkingSpeed, InputSpeedRange.Min, InputSpeedRange.Max, "%.2f");

            ImGui::PopItemWidth();
        }

        // Parameters are applied automatically in UpdateController
        ImGui::Separator();
        ImGui::TextDisabled("Parameters are applied automatically during update");
    }

    Common::CaseRenderResult CaseBVHMotionMatching::OnRender(std::pair<std::uint32_t, std::uint32_t> const desiredSize) {
        _frame.Resize(desiredSize, _enableMSAA ? 4 : 1);

        _cameraManager.Update(_camera);

        // Update Controller
        float dt = ImGui::GetIO().DeltaTime;
        UpdateController(dt);

        _program.GetUniforms().SetByName("mvp", _camera.GetProjectionMatrix((float(desiredSize.first) / desiredSize.second)) * _camera.GetViewMatrix());
        _program.GetUniforms().SetByName("matModel", glm::mat4(1.0f));
        _program.GetUniforms().SetByName("matNormal", glm::mat4(1.0f));
        _program.GetUniforms().SetByName("colDiffuse", glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

        gl_using(_frame);
        glEnable(GL_DEPTH_TEST);
        glClearColor(0.9f, 0.9f, 0.9f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Render Character (Static for now)
        gl_using(_program);
        glBindVertexArray(_vao.Get());
        glDrawElements(GL_TRIANGLES, _character.triangles.size, GL_UNSIGNED_SHORT, 0);
        glBindVertexArray(0);

        // Render Ground
        glm::mat4 view = _camera.GetViewMatrix();
        glm::mat4 proj = _camera.GetProjectionMatrix((float(desiredSize.first) / desiredSize.second));
        glm::mat4 mvp  = proj * view;
        _sceneEnv.Render(mvp, _showAxis);

        // Render Simulation Object
        _simulationObject.Render(
            view,
            proj,
            glm::vec3(_position.x, _position.y, _position.z),
            glm::quat(_rotation.w, _rotation.x, _rotation.y, _rotation.z),
            _predictedPositions,
            _predictedRotations,
            glm::vec3(_desiredVelocity.x, _desiredVelocity.y, _desiredVelocity.z));

        glDisable(GL_DEPTH_TEST);

        return Common::CaseRenderResult {
            .Fixed     = false,
            .Flipped   = true,
            .Image     = _frame.GetColorAttachment(),
            .ImageSize = desiredSize,
        };
    }

    void CaseBVHMotionMatching::OnProcessInput(ImVec2 const & pos) {
        _cameraManager.EnableKeyboardPan = ! _controlCharacter;
        _cameraManager.ProcessInput(_camera, pos);
    }
} // namespace VCX::Labs::MotionMatching
