#include "Labs/MotionMatching/CaseBVH.h"
#include "Assets/bundled.h"
#include "Labs/MotionMatching/Core/Animation/bone_operations.hpp"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace VCX::Labs::MotionMatching {

    struct Vertex {
        glm::vec3 Position;
        glm::vec3 Normal;
    };

    struct VertexColor {
        glm::vec3 Position;
        glm::vec3 Color;
    };

    CaseBVH::CaseBVH():
        _program(
            Engine::GL::UniqueProgram({ Engine::GL::SharedShader("assets/shaders/skeleton.vert"), Engine::GL::SharedShader("assets/shaders/skeleton.frag") })),
        _programGround(
            Engine::GL::UniqueProgram({ Engine::GL::SharedShader("assets/shaders/checkerboard.vert"), Engine::GL::SharedShader("assets/shaders/checkerboard.frag") })),
        _programFlat(
            Engine::GL::UniqueProgram({ Engine::GL::SharedShader("assets/shaders/flat.vert"), Engine::GL::SharedShader("assets/shaders/flat.frag") })) {
        _cameraManager.AutoRotate = false;
        _cameraManager.Save(_camera);

        // Define Unit Cube (24 vertices, 36 indices)
        // Positions
        glm::vec3 p0(-0.5f, -0.5f, 0.5f);
        glm::vec3 p1(0.5f, -0.5f, 0.5f);
        glm::vec3 p2(0.5f, 0.5f, 0.5f);
        glm::vec3 p3(-0.5f, 0.5f, 0.5f);
        glm::vec3 p4(-0.5f, -0.5f, -0.5f);
        glm::vec3 p5(0.5f, -0.5f, -0.5f);
        glm::vec3 p6(0.5f, 0.5f, -0.5f);
        glm::vec3 p7(-0.5f, 0.5f, -0.5f);

        // Normals
        glm::vec3 nFront(0.0f, 0.0f, 1.0f);
        glm::vec3 nBack(0.0f, 0.0f, -1.0f);
        glm::vec3 nLeft(-1.0f, 0.0f, 0.0f);
        glm::vec3 nRight(1.0f, 0.0f, 0.0f);
        glm::vec3 nTop(0.0f, 1.0f, 0.0f);
        glm::vec3 nBottom(0.0f, -1.0f, 0.0f);

        std::vector<Vertex> vertices = {
            // Front
            { p0,  nFront },
            { p1,  nFront },
            { p2,  nFront },
            { p3,  nFront },
            // Back
            { p5,   nBack },
            { p4,   nBack },
            { p7,   nBack },
            { p6,   nBack },
            // Left
            { p4,   nLeft },
            { p0,   nLeft },
            { p3,   nLeft },
            { p7,   nLeft },
            // Right
            { p1,  nRight },
            { p5,  nRight },
            { p6,  nRight },
            { p2,  nRight },
            // Top
            { p3,    nTop },
            { p2,    nTop },
            { p6,    nTop },
            { p7,    nTop },
            // Bottom
            { p4, nBottom },
            { p5, nBottom },
            { p1, nBottom },
            { p0, nBottom }
        };

        std::vector<uint32_t> indices = {
            0, 1, 2, 2, 3, 0, // Front
            4,
            5,
            6,
            6,
            7,
            4, // Back
            8,
            9,
            10,
            10,
            11,
            8, // Left
            12,
            13,
            14,
            14,
            15,
            12, // Right
            16,
            17,
            18,
            18,
            19,
            16, // Top
            20,
            21,
            22,
            22,
            23,
            20 // Bottom
        };

        _indexCount = indices.size();

        glBindVertexArray(_vao.Get());

        glBindBuffer(GL_ARRAY_BUFFER, _vboMesh.Get());
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

        // Position (loc 0)
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *) 0);

        // Normal (loc 1)
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *) offsetof(Vertex, Normal));

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _eboMesh.Get());
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), indices.data(), GL_STATIC_DRAW);

        // Instance Buffer (loc 2, 3, 4, 5 for mat4)
        glBindBuffer(GL_ARRAY_BUFFER, _vboInstance.Get());
        // Initial allocation, will be updated every frame
        // We don't know the size yet, but we can allocate some initial size or just leave it empty for now.
        // But we need to set up pointers.

        std::size_t vec4Size = sizeof(glm::vec4);
        for (int i = 0; i < 4; ++i) {
            glEnableVertexAttribArray(2 + i);
            glVertexAttribPointer(2 + i, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void *) (i * vec4Size));
            glVertexAttribDivisor(2 + i, 1);
        }

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        // Ground
        std::vector<Vertex> groundVertices = {
            { { -100.0f, 0.0f, -100.0f }, { 0.0f, 1.0f, 0.0f } },
            {  { 100.0f, 0.0f, -100.0f }, { 0.0f, 1.0f, 0.0f } },
            {   { 100.0f, 0.0f, 100.0f }, { 0.0f, 1.0f, 0.0f } },
            { { -100.0f, 0.0f, -100.0f }, { 0.0f, 1.0f, 0.0f } },
            {   { 100.0f, 0.0f, 100.0f }, { 0.0f, 1.0f, 0.0f } },
            {  { -100.0f, 0.0f, 100.0f }, { 0.0f, 1.0f, 0.0f } },
        };

        glBindVertexArray(_vaoGround.Get());
        glBindBuffer(GL_ARRAY_BUFFER, _vboGround.Get());
        glBufferData(GL_ARRAY_BUFFER, groundVertices.size() * sizeof(Vertex), groundVertices.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *) 0);

        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *) offsetof(Vertex, Normal));

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        // Axis & Dot
        std::vector<VertexColor> axisVertices;

        auto addBox = [&](glm::vec3 center, glm::vec3 size, glm::vec3 color) {
            glm::vec3 half = size * 0.5f;
            glm::vec3 p[8];
            p[0] = center + glm::vec3(-half.x, -half.y, half.z);
            p[1] = center + glm::vec3(half.x, -half.y, half.z);
            p[2] = center + glm::vec3(half.x, half.y, half.z);
            p[3] = center + glm::vec3(-half.x, half.y, half.z);
            p[4] = center + glm::vec3(-half.x, -half.y, -half.z);
            p[5] = center + glm::vec3(half.x, -half.y, -half.z);
            p[6] = center + glm::vec3(half.x, half.y, -half.z);
            p[7] = center + glm::vec3(-half.x, half.y, -half.z);

            auto addQuad = [&](int a, int b, int c, int d) {
                axisVertices.push_back({ p[a], color });
                axisVertices.push_back({ p[b], color });
                axisVertices.push_back({ p[c], color });
                axisVertices.push_back({ p[c], color });
                axisVertices.push_back({ p[d], color });
                axisVertices.push_back({ p[a], color });
            };
            addQuad(0, 1, 2, 3);
            addQuad(5, 4, 7, 6);
            addQuad(4, 0, 3, 7);
            addQuad(1, 5, 6, 2);
            addQuad(3, 2, 6, 7);
            addQuad(4, 5, 1, 0);
        };

        auto addCone = [&](glm::vec3 baseCenter, float radius, float height, glm::vec3 axis, glm::vec3 color) {
            int       segments = 16;
            glm::vec3 tip      = baseCenter + axis * height;
            glm::vec3 u        = (std::abs(axis.y) > 0.9f) ? glm::vec3(1, 0, 0) : glm::vec3(0, 1, 0);
            glm::vec3 v        = glm::normalize(glm::cross(axis, u));
            u                  = glm::cross(v, axis);

            for (int i = 0; i < segments; ++i) {
                float     angle1 = (float) i / segments * 2.0f * glm::pi<float>();
                float     angle2 = (float) (i + 1) / segments * 2.0f * glm::pi<float>();
                glm::vec3 p1     = baseCenter + radius * (u * std::cos(angle1) + v * std::sin(angle1));
                glm::vec3 p2     = baseCenter + radius * (u * std::cos(angle2) + v * std::sin(angle2));

                axisVertices.push_back({ p1, color });
                axisVertices.push_back({ p2, color });
                axisVertices.push_back({ tip, color });
                axisVertices.push_back({ baseCenter, color });
                axisVertices.push_back({ p2, color });
                axisVertices.push_back({ p1, color });
            }
        };

        auto addSphere = [&](glm::vec3 center, float radius, glm::vec3 color) {
            int stacks = 10;
            int slices = 10;
            for (int i = 0; i < stacks; ++i) {
                float phi1 = (float) i / stacks * glm::pi<float>();
                float phi2 = (float) (i + 1) / stacks * glm::pi<float>();
                for (int j = 0; j < slices; ++j) {
                    float theta1 = (float) j / slices * 2.0f * glm::pi<float>();
                    float theta2 = (float) (j + 1) / slices * 2.0f * glm::pi<float>();
                    auto  getPos = [&](float phi, float theta) {
                        return center + radius * glm::vec3(std::sin(phi) * std::cos(theta), std::cos(phi), std::sin(phi) * std::sin(theta));
                    };
                    glm::vec3 p1 = getPos(phi1, theta1);
                    glm::vec3 p2 = getPos(phi1, theta2);
                    glm::vec3 p3 = getPos(phi2, theta2);
                    glm::vec3 p4 = getPos(phi2, theta1);
                    axisVertices.push_back({ p1, color });
                    axisVertices.push_back({ p2, color });
                    axisVertices.push_back({ p3, color });
                    axisVertices.push_back({ p3, color });
                    axisVertices.push_back({ p4, color });
                    axisVertices.push_back({ p1, color });
                }
            }
        };

        addBox(glm::vec3(0.45f, 0.0f, 0.0f), glm::vec3(0.9f, 0.02f, 0.02f), glm::vec3(1, 0, 0));
        addCone(glm::vec3(0.9f, 0.0f, 0.0f), 0.04f, 0.1f, glm::vec3(1, 0, 0), glm::vec3(1, 0, 0));

        addBox(glm::vec3(0.0f, 0.45f, 0.0f), glm::vec3(0.02f, 0.9f, 0.02f), glm::vec3(0, 1, 0));
        addCone(glm::vec3(0.0f, 0.9f, 0.0f), 0.04f, 0.1f, glm::vec3(0, 1, 0), glm::vec3(0, 1, 0));

        addBox(glm::vec3(0.0f, 0.0f, 0.45f), glm::vec3(0.02f, 0.02f, 0.9f), glm::vec3(0, 0, 1));
        addCone(glm::vec3(0.0f, 0.0f, 0.9f), 0.04f, 0.1f, glm::vec3(0, 0, 1), glm::vec3(0, 0, 1));

        addSphere(glm::vec3(0, 0, 0), 0.05f, glm::vec3(0, 0, 0));

        _axisVertexCount = axisVertices.size();
        glBindVertexArray(_vaoAxis.Get());
        glBindBuffer(GL_ARRAY_BUFFER, _vboAxis.Get());
        glBufferData(GL_ARRAY_BUFFER, axisVertices.size() * sizeof(VertexColor), axisVertices.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VertexColor), (void *) 0);

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(VertexColor), (void *) offsetof(VertexColor, Color));

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    void CaseBVH::OnBVHLoaded() {
        _joints = _bvh->GetJointList();

        int numJoints = _joints.size();
        _boneParents.resize(numJoints);
        _localPositions.resize(numJoints);
        _localRotations.resize(numJoints);
        _globalPositions.resize(numJoints);
        _globalRotations.resize(numJoints);

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

        _instances.clear();
        for (int i = 0; i < _joints.size(); ++i) {
            int parentIdx = _boneParents(i);
            if (parentIdx != -1) {
                auto pPos = _globalPositions(parentIdx);
                auto cPos = _globalPositions(i);

                glm::vec3 p(pPos.x, pPos.y, pPos.z);
                glm::vec3 c(cPos.x, cPos.y, cPos.z);

                glm::vec3 center = (p + c) * 0.5f;
                glm::vec3 axis   = c - p;
                float     length = glm::length(axis);

                if (length < 1e-5f) continue;

                glm::vec3 mainAxis(0, 1, 0); // The cube's local Y axis
                glm::vec3 targetAxis = glm::normalize(axis);

                // Handle parallel vectors for rotation
                glm::quat rotation;
                if (glm::abs(glm::dot(mainAxis, targetAxis)) > 0.9999f) {
                    if (glm::dot(mainAxis, targetAxis) > 0)
                        rotation = glm::quat(1, 0, 0, 0);
                    else
                        rotation = glm::angleAxis(glm::pi<float>(), glm::vec3(1, 0, 0));
                } else {
                    rotation = glm::rotation(mainAxis, targetAxis);
                }

                // Scale: width=0.05, length=length
                glm::vec3 scale(0.05f, length, 0.05f);

                glm::mat4 model = glm::translate(glm::mat4(1.0f), center) * glm::toMat4(rotation) * glm::scale(glm::mat4(1.0f), scale);

                _instances.push_back(model);
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
            _instances.clear();
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

        ImGui::Checkbox("Anti-aliasing", &_enableMSAA);
        ImGui::SameLine();
        ImGui::Checkbox("Show Axis", &_showAxis);

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

        _frame.Resize(desiredSize, _enableMSAA ? 4 : 1);

        _cameraManager.Update(_camera);
        _program.GetUniforms().SetByName("u_Projection", _camera.GetProjectionMatrix((float(desiredSize.first) / desiredSize.second)));
        _program.GetUniforms().SetByName("u_View", _camera.GetViewMatrix());
        _program.GetUniforms().SetByName("u_Color", glm::vec3(121.0f / 255, 207.0f / 255, 171.0f / 255));
        _program.GetUniforms().SetByName("u_LightDir", glm::vec3(1.0f, 1.0f, 1.0f));

        gl_using(_frame);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glDisable(GL_CULL_FACE);
        glDisable(GL_BLEND);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDepthMask(GL_TRUE);

        // Clear the framebuffer
        glClearColor(0.9f, 0.9f, 0.9f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (! _stopped) {
            UpdateFrame(ImGui::GetIO().DeltaTime);
        } else {
            // Still update to render current frame if paused
            UpdateFrame(0.0f);
        }

        if (! _instances.empty()) {
            gl_using(_program);

            glBindBuffer(GL_ARRAY_BUFFER, _vboInstance.Get());
            glBufferData(GL_ARRAY_BUFFER, _instances.size() * sizeof(glm::mat4), _instances.data(), GL_DYNAMIC_DRAW);

            glBindVertexArray(_vao.Get());
            glDrawElementsInstanced(GL_TRIANGLES, _indexCount, GL_UNSIGNED_INT, 0, _instances.size());
            glBindVertexArray(0);
        }

        // mvp for render
        glm::mat4 mvp = _camera.GetProjectionMatrix((float(desiredSize.first) / desiredSize.second)) * _camera.GetViewMatrix();

        // Render Ground
        _programGround.GetUniforms().SetByName("mvp", mvp);
        _programGround.GetUniforms().SetByName("matModel", glm::mat4(1.0f));
        _programGround.GetUniforms().SetByName("matNormal", glm::transpose(glm::inverse(glm::mat4(1.0f))));

        gl_using(_programGround);
        glBindVertexArray(_vaoGround.Get());
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);

        // Render Axis & Dot
        if (_showAxis) {
            glDisable(GL_DEPTH_TEST);
            _programFlat.GetUniforms().SetByName("u_MVP", mvp);
            gl_using(_programFlat);
            glBindVertexArray(_vaoAxis.Get());
            glDrawArrays(GL_TRIANGLES, 0, _axisVertexCount);
            glBindVertexArray(0);
            glEnable(GL_DEPTH_TEST);
        }

        glDisable(GL_DEPTH_TEST);

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
