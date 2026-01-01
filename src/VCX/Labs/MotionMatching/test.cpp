#include "Assets/bundled.h"
#include "Engine/app.h"
#include "Engine/Camera.hpp"
#include "Engine/GL/Frame.hpp"
#include "Labs/Common/ICase.h"
#include "Labs/Common/OrbitCameraManager.h"
#include "Labs/Common/UI.h"
#include "Labs/MotionMatching/Core/Animation/controller.hpp"
#include "Labs/MotionMatching/SceneEnvironment.hpp"
#include "Labs/MotionMatching/SimulationObject.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include <iostream>

using namespace VCX::Labs::MotionMatching;
using namespace VCX::Labs::MotionMatching::Core::Math;
using namespace VCX::Labs::MotionMatching::Core::Animation;

class CaseTest : public VCX::Labs::Common::ICase {
public:
    CaseTest() {
        _cameraManager.AutoRotate = false;
        _cameraManager.EnablePan  = false; // Disable camera panning to avoid conflict with WASD
        _cameraManager.Save(_camera);

        _position        = vec3(0, 0, 0);
        _velocity        = vec3(0, 0, 0);
        _acceleration    = vec3(0, 0, 0);
        _rotation        = quat(1, 0, 0, 0);
        _angularVelocity = vec3(0, 0, 0);
    }

    virtual std::string_view const GetName() override { return "Controller Test"; }

    virtual void OnSetupPropsUI() override {
        ImGui::Text("Use WASD to move");
        ImGui::DragFloat("Halflife", &_halflife, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("Speed", &_speed, 0.1f, 0.0f, 10.0f);

        ImGui::Separator();
        ImGui::Text("Position: %.2f, %.2f, %.2f", _position.x, _position.y, _position.z);
        ImGui::Text("Velocity: %.2f, %.2f, %.2f", _velocity.x, _velocity.y, _velocity.z);

        if (ImGui::Button("Reset")) {
            _position        = vec3(0, 0, 0);
            _velocity        = vec3(0, 0, 0);
            _acceleration    = vec3(0, 0, 0);
            _rotation        = quat(1, 0, 0, 0);
            _angularVelocity = vec3(0, 0, 0);
        }
    }

    virtual VCX::Labs::Common::CaseRenderResult OnRender(std::pair<std::uint32_t, std::uint32_t> const desiredSize) override {
        _frame.Resize(desiredSize);

        // Update Camera
        _cameraManager.Update(_camera);

        // Update Controller
        float dt = ImGui::GetIO().DeltaTime;
        UpdateController(dt);

        // Render Scene
        gl_using(_frame);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glDisable(GL_CULL_FACE);
        glDisable(GL_BLEND);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDepthMask(GL_TRUE);

        glClearColor(0.9f, 0.9f, 0.9f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 view = _camera.GetViewMatrix();
        glm::mat4 proj = _camera.GetProjectionMatrix((float(desiredSize.first) / desiredSize.second));
        glm::mat4 mvp  = proj * view;

        _sceneEnv.Render(mvp, true);
        _simulationObject.Render(
            view,
            proj,
            glm::vec3(_position.x, _position.y, _position.z),
            glm::quat(_rotation.w, _rotation.x, _rotation.y, _rotation.z),
            _predictedPositions,
            _predictedRotations);

        return VCX::Labs::Common::CaseRenderResult {
            .Fixed     = false,
            .Flipped   = true,
            .Image     = _frame.GetColorAttachment(),
            .ImageSize = desiredSize,
        };
    }

    virtual void OnProcessInput(ImVec2 const & pos) override {
        _cameraManager.ProcessInput(_camera, pos);

        // Draw visualization here
        ImDrawList * drawList = ImGui::GetWindowDrawList();

        // Get the window size and position from ImGui (current window is the one holding the image)
        ImVec2 contentMin  = ImGui::GetItemRectMin();
        ImVec2 contentSize = ImGui::GetItemRectSize();

        glm::mat4 view = _camera.GetViewMatrix();
        glm::mat4 proj = _camera.GetProjectionMatrix(contentSize.x / contentSize.y);
        glm::vec4 viewport(0, 0, contentSize.x, contentSize.y);

        glm::vec3 charPos(_position.x, _position.y, _position.z);
        glm::vec3 projected = glm::project(charPos, view, proj, viewport);

        // projected.x is 0..width, projected.y is 0..height (bottom-up)
        // We need to convert to screen coordinates.
        // Screen X = contentMin.x + projected.x
        // Screen Y = contentMin.y + (height - projected.y)

        if (projected.z >= 0.0f && projected.z <= 1.0f) {
            float screenX = contentMin.x + projected.x;
            float screenY = contentMin.y + (contentSize.y - projected.y);

            drawList->AddCircleFilled(ImVec2(screenX, screenY), 10.0f, IM_COL32(255, 0, 0, 255));

            // Draw direction
            vec3      forward = quat_mul_vec3(_rotation, vec3(0, 0, 1));
            glm::vec3 dirPos  = charPos + glm::vec3(forward.x, forward.y, forward.z) * 1.0f;
            glm::vec3 dirProj = glm::project(dirPos, view, proj, viewport);

            if (dirProj.z >= 0.0f && dirProj.z <= 1.0f) {
                float dirScreenX = contentMin.x + dirProj.x;
                float dirScreenY = contentMin.y + (contentSize.y - dirProj.y);

                drawList->AddLine(ImVec2(screenX, screenY), ImVec2(dirScreenX, dirScreenY), IM_COL32(0, 255, 0, 255), 2.0f);
            }
        }
    }

private:
    void UpdateController(float dt) {
        // Input
        float x = 0.0f;
        float y = 0.0f;
        if (ImGui::IsKeyDown(ImGuiKey_W)) y += 1.0f;
        if (ImGui::IsKeyDown(ImGuiKey_S)) y -= 1.0f;
        if (ImGui::IsKeyDown(ImGuiKey_A)) x += 1.0f; // Left
        if (ImGui::IsKeyDown(ImGuiKey_D)) x -= 1.0f; // Right

        vec3 stick = gamepad_get_stick(x, y);

        // Get camera azimuth
        // Camera direction is Target - Eye
        glm::vec3 camDir        = glm::normalize(_camera.Target - _camera.Eye);
        float     cameraAzimuth = std::atan2(camDir.x, camDir.z);

        vec3 desiredVel = desired_velocity_update(
            stick,
            cameraAzimuth,
            _rotation,
            _speed,
            _speed,
            _speed);

        quat desiredRot = desired_rotation_update(
            _rotation,
            stick,
            vec3(0, 0, 0),
            cameraAzimuth,
            false,
            desiredVel);

        // Simulation Update
        simulation_positions_update(
            _position,
            _velocity,
            _acceleration,
            desiredVel,
            _halflife,
            dt);

        simulation_rotations_update(
            _rotation,
            _angularVelocity,
            desiredRot,
            _halflife,
            dt);

        // Predict Future Trajectory
        _predictedPositions.clear();
        _predictedRotations.clear();

        vec3 predPos    = _position;
        vec3 predVel    = _velocity;
        vec3 predAcc    = _acceleration;
        quat predRot    = _rotation;
        vec3 predAngVel = _angularVelocity;

        // Predict 1 second into the future (assuming 60fps for prediction steps)
        float predDt = 1.0f / 60.0f;
        int   steps  = 60;

        for (int i = 0; i < steps; ++i) {
            // Assume desired velocity/rotation remains constant based on current input
            // Note: In a real scenario, desiredVel might change if it depends on rotation which changes.
            // Here we re-calculate desiredVel/Rot based on the predicted rotation if needed,
            // but for simple inertial prediction, keeping them constant or re-evaluating is a choice.
            // Let's re-evaluate to be more accurate to the controller logic.

            vec3 predDesiredVel = desired_velocity_update(
                stick,
                cameraAzimuth,
                predRot,
                _speed,
                _speed,
                _speed);

            quat predDesiredRot = desired_rotation_update(
                predRot,
                stick,
                vec3(0, 0, 0),
                cameraAzimuth,
                false,
                predDesiredVel);

            simulation_positions_update(
                predPos,
                predVel,
                predAcc,
                predDesiredVel,
                _halflife,
                predDt);

            simulation_rotations_update(
                predRot,
                predAngVel,
                predDesiredRot,
                _halflife,
                predDt);

            // Store every few steps to avoid too dense rendering? Or all steps.
            // Let's store every 5th step (approx every 0.08s)
            if (i % 5 == 0) {
                _predictedPositions.push_back(glm::vec3(predPos.x, predPos.y, predPos.z));
                _predictedRotations.push_back(glm::quat(predRot.w, predRot.x, predRot.y, predRot.z));
            }
        }
    }

    VCX::Engine::GL::UniqueRenderFrame    _frame;
    SceneEnvironment                      _sceneEnv;
    SimulationObject                      _simulationObject;
    VCX::Engine::Camera                   _camera { .Eye = glm::vec3(0, 5, 5), .Target = glm::vec3(0, 0, 0) };
    VCX::Labs::Common::OrbitCameraManager _cameraManager;

    vec3 _position;
    vec3 _velocity;
    vec3 _acceleration;
    quat _rotation;
    vec3 _angularVelocity;

    std::vector<glm::vec3> _predictedPositions;
    std::vector<glm::quat> _predictedRotations;

    float _halflife = 0.2f;
    float _speed    = 5.0f;
};

class TestApp : public VCX::Engine::IApp {
private:
    VCX::Labs::Common::UI                                         _ui;
    CaseTest                                                      _caseTest;
    std::size_t                                                   _caseId = 0;
    std::vector<std::reference_wrapper<VCX::Labs::Common::ICase>> _cases  = { _caseTest };

public:
    TestApp(): _ui(VCX::Labs::Common::UIOptions {}) {}

    void OnFrame() override {
        _ui.Setup(_cases, _caseId);
    }
};

int main() {
    using namespace VCX;
    return Engine::RunApp<TestApp>(Engine::AppContextOptions {
        .Title         = "Motion Matching Controller Test",
        .WindowSize    = { 1024, 768 },
        .FontSize      = 16,
        .IconFileNames = Assets::DefaultIcons,
        .FontFileNames = Assets::DefaultFonts,
    });
}
