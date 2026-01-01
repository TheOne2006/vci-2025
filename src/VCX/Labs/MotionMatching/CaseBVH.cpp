#include "Labs/MotionMatching/CaseBVH.h"
#include "Assets/bundled.h"
#include "Labs/MotionMatching/Core/Animation/bone_operations.hpp"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace VCX::Labs::MotionMatching {

    BoxRenderer::BoxRenderer():
        CenterPosition(0, 0, 0),
        MainAxis(0, 1, 0),
        BoxItem(Engine::GL::VertexLayout().Add<glm::vec3>("position", Engine::GL::DrawFrequency::Stream, 0), Engine::GL::PrimitiveType::Triangles),
        LineItem(Engine::GL::VertexLayout().Add<glm::vec3>("position", Engine::GL::DrawFrequency::Stream, 0), Engine::GL::PrimitiveType::Lines) {
        //     3-----2
        //    /|    /|
        //   0 --- 1 |
        //   | 7 - | 6
        //   |/    |/
        //   4 --- 5
        VertsPosition.resize(8);
        const std::vector<std::uint32_t> line_index = { 0, 1, 1, 2, 2, 3, 3, 0, 4, 5, 5, 6, 6, 7, 7, 4, 0, 4, 1, 5, 2, 6, 3, 7 }; // line index
        LineItem.UpdateElementBuffer(line_index);

        const std::vector<std::uint32_t> tri_index = { 0, 1, 2, 0, 2, 3, 1, 4, 0, 1, 4, 5, 1, 6, 5, 1, 2, 6, 2, 3, 7, 2, 6, 7, 0, 3, 7, 0, 4, 7, 4, 5, 6, 4, 6, 7 };
        BoxItem.UpdateElementBuffer(tri_index);
    }

    void BoxRenderer::render(Engine::GL::UniqueProgram & program) {
        auto span_bytes = Engine::make_span_bytes<glm::vec3>(VertsPosition);

        program.GetUniforms().SetByName("u_Color", glm::vec3(121.0f / 255, 207.0f / 255, 171.0f / 255));
        BoxItem.UpdateVertexBuffer("position", span_bytes);
        BoxItem.Draw({ program.Use() });

        program.GetUniforms().SetByName("u_Color", glm::vec3(1.0f, 1.0f, 1.0f));
        LineItem.UpdateVertexBuffer("position", span_bytes);
        LineItem.Draw({ program.Use() });
    }

    void BoxRenderer::calc_vert_position() {
        glm::vec3         new_y = glm::normalize(MainAxis);
        glm::quat         quat  = glm::rotation(glm::vec3(0, 1, 0), new_y);
        glm::vec3         new_x = quat * glm::vec3(0.5f * width, 0.0f, 0.0f);
        glm::vec3         new_z = quat * glm::vec3(0.0f, 0.0f, 0.5f * width);
        const glm::vec3 & c     = CenterPosition;
        new_y *= 0.5 * length;
        VertsPosition[0] = c - new_x + new_y + new_z;
        VertsPosition[1] = c + new_x + new_y + new_z;
        VertsPosition[2] = c + new_x + new_y - new_z;
        VertsPosition[3] = c - new_x + new_y - new_z;
        VertsPosition[4] = c - new_x - new_y + new_z;
        VertsPosition[5] = c + new_x - new_y + new_z;
        VertsPosition[6] = c + new_x - new_y - new_z;
        VertsPosition[7] = c - new_x - new_y - new_z;
    }

    CaseBVH::CaseBVH():
        _program(
            Engine::GL::UniqueProgram({ Engine::GL::SharedShader("assets/shaders/flat.vert"), Engine::GL::SharedShader("assets/shaders/flat.frag") })) {
        _cameraManager.AutoRotate = false;
        _cameraManager.Save(_camera);
    }

    void CaseBVH::OnBVHLoaded() {
        _joints = _bvh->GetJointList();

        int numJoints = _joints.size();
        _boneParents.resize(numJoints);
        _localPositions.resize(numJoints);
        _localRotations.resize(numJoints);
        _globalPositions.resize(numJoints);
        _globalRotations.resize(numJoints);

        _boneRenderers.clear();

        for (int i = 0; i < numJoints; ++i) {
            auto parent = _joints[i]->parent();
            if (parent) {
                // Find parent index
                int parentIndex = -1;
                for (int j = 0; j < numJoints; ++j) {
                    if (_joints[j] == parent) {
                        parentIndex = j;
                        break;
                    }
                }
                _boneParents(i) = parentIndex;
                _boneRenderers.emplace_back(); // Create renderer for this bone (connection to parent)
            } else {
                _boneParents(i) = -1;
            }
        }

        _currentTime = 0.0f;
        _frameIndex  = 0;
    }

    void CaseBVH::UpdateFrame(float dt) {
        if (! _bvh) return;

        _currentTime += dt;
        float totalDuration = _bvh->frames() * _bvh->frame_time();
        if (_currentTime > totalDuration) {
            _currentTime = std::fmod(_currentTime, totalDuration);
        }

        _frameIndex = static_cast<int>(_currentTime / _bvh->frame_time());
        _frameIndex = std::clamp(_frameIndex, 0, _bvh->frames() - 1);

        for (int i = 0; i < _joints.size(); ++i) {
            auto               transform = _bvh->GetTransformationRelativeToParent(_joints[i], _frameIndex);
            Eigen::Vector3d    pos       = transform.translation();
            Eigen::Quaterniond rot(transform.rotation());

            _localPositions(i) = Core::Math::vec3(pos.x(), pos.y(), pos.z()) * 0.01f;
            _localRotations(i) = Core::Math::quat(rot.w(), rot.x(), rot.y(), rot.z());
        }

        Core::Animation::forward_kinematics_full(
            _globalPositions,
            _globalRotations,
            _localPositions,
            _localRotations,
            _boneParents);

        // Update renderers
        int rendererIdx = 0;
        for (int i = 0; i < _joints.size(); ++i) {
            int parentIdx = _boneParents(i);
            if (parentIdx != -1) {
                auto & renderer = _boneRenderers[rendererIdx++];

                auto pPos = _globalPositions(parentIdx);
                auto cPos = _globalPositions(i);

                glm::vec3 p(pPos.x, pPos.y, pPos.z);
                glm::vec3 c(cPos.x, cPos.y, cPos.z);

                renderer.CenterPosition = (p + c) * 0.5f;
                glm::vec3 axis          = c - p;
                renderer.length         = glm::length(axis);
                if (renderer.length > 1e-5f) {
                    renderer.MainAxis = glm::normalize(axis);
                } else {
                    renderer.MainAxis = glm::vec3(0, 1, 0);
                }
                renderer.calc_vert_position();
            }
        }
    }

    void CaseBVH::OnSetupPropsUI() {
        const char * bvhNames[] = {
            "Aiming", "Dance", "FallAndGetUp", "Fight", "Ground", "MultipleActions", "Obstacles", "PushAndStumble", "Run", "Walk"
        };

        if (ImGui::Combo("BVH File", &_currentBVH, bvhNames, IM_ARRAYSIZE(bvhNames))) {
            _bvh = nullptr;
            _joints.clear();
            _boneRenderers.clear();
        }

        if (_loadFuture.valid()) {
            ImGui::Text("Loading %s...", bvhNames[_currentBVH]);
            ImGui::SameLine();
        } else {
            if (ImGui::Button("Load")) {
                std::string path = std::string(VCX::Assets::BVHFiles[_currentBVH]);
                _loadFuture      = std::async(std::launch::async, [path]() {
                    return std::make_unique<bvh11::BvhObject>(path);
                });
            }
            ImGui::SameLine();
        }

        if (ImGui::Button(_stopped ? "Start" : "Pause")) _stopped = ! _stopped;
        ImGui::SameLine();
        if (ImGui::Button("Reset")) {
            _currentTime = 0.0f;
            _frameIndex  = 0;
        }

        if (_bvh) {
            if (ImGui::SliderInt("Frame", &_frameIndex, 0, _bvh->frames() - 1)) {
                _currentTime = _frameIndex * _bvh->frame_time();
            }
        }
    }

    Common::CaseRenderResult CaseBVH::OnRender(std::pair<std::uint32_t, std::uint32_t> const desiredSize) {
        if (_loadFuture.valid() && _loadFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            _bvh = _loadFuture.get();
            OnBVHLoaded();
        }

        _frame.Resize(desiredSize);

        _cameraManager.Update(_camera);
        _program.GetUniforms().SetByName("u_Projection", _camera.GetProjectionMatrix((float(desiredSize.first) / desiredSize.second)));
        _program.GetUniforms().SetByName("u_View", _camera.GetViewMatrix());

        gl_using(_frame);
        glEnable(GL_LINE_SMOOTH);
        glLineWidth(0.5f);
        glPointSize(4.f);

        if (! _stopped) {
            UpdateFrame(ImGui::GetIO().DeltaTime);
        } else {
            // Still update to render current frame if paused
            UpdateFrame(0.0f);
        }

        for (auto & renderer : _boneRenderers) {
            renderer.render(_program);
        }

        glLineWidth(1.f);
        glPointSize(1.f);
        glDisable(GL_LINE_SMOOTH);

        return Common::CaseRenderResult {
            .Fixed     = false,
            .Flipped   = true,
            .Image     = _frame.GetColorAttachment(),
            .ImageSize = desiredSize,
        };
    }

    void CaseBVH::OnProcessInput(ImVec2 const & pos) {
        _cameraManager.ProcessInput(_camera, pos);
    }
} // namespace VCX::Labs::MotionMatching
