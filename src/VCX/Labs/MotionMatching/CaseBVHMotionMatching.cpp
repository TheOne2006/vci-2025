#include "Labs/MotionMatching/CaseBVHMotionMatching.h"
#include "Assets/bundled.h"
#include "Labs/MotionMatching/Core/Animation/bone_operations.hpp"
#include "Labs/MotionMatching/Core/Animation/character.hpp"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace VCX::Labs::MotionMatching {

    struct VertexColor {
        glm::vec3 Position;
        glm::vec3 Color;
    };

    struct Vertex {
        glm::vec3 Position;
        glm::vec3 Normal;
    };

    CaseBVHMotionMatching::CaseBVHMotionMatching():
        _program(
            Engine::GL::UniqueProgram({ Engine::GL::SharedShader("assets/shaders/character.vert"), Engine::GL::SharedShader("assets/shaders/character.frag") })),
        _programFlat(
            Engine::GL::UniqueProgram({ Engine::GL::SharedShader("assets/shaders/flat.vert"), Engine::GL::SharedShader("assets/shaders/flat.frag") })),
        _programGround(
            Engine::GL::UniqueProgram({ Engine::GL::SharedShader("assets/shaders/checkerboard.vert"), Engine::GL::SharedShader("assets/shaders/checkerboard.frag") })) {
        _cameraManager.AutoRotate = false;
        _cameraManager.Save(_camera);

        // Load Character
        Core::Animation::character_load(_character, VCX::Assets::CharacterPath[0].data());

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

        // Ground
        std::vector<Vertex> groundVertices = {
            { { -100.0f, 0.0f, -100.0f }, { 0.0f, 1.0f, 0.0f } },
            {  { 100.0f, 0.0f, -100.0f }, { 0.0f, 1.0f, 0.0f } },
            {   { 100.0f, 0.0f, 100.0f }, { 0.0f, 1.0f, 0.0f } },
            { { -100.0f, 0.0f, -100.0f }, { 0.0f, 1.0f, 0.0f } },
            {   { 100.0f, 0.0f, 100.0f }, { 0.0f, 1.0f, 0.0f } },
            {  { -100.0f, 0.0f, 100.0f }, { 0.0f, 1.0f, 0.0f } }
        };

        glBindVertexArray(_vaoGround.Get());
        glBindBuffer(GL_ARRAY_BUFFER, _vboGround.Get());
        glBufferData(GL_ARRAY_BUFFER, groundVertices.size() * sizeof(Vertex), groundVertices.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *) offsetof(Vertex, Position));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *) offsetof(Vertex, Normal));

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

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

    void CaseBVHMotionMatching::OnBVHLoaded() {
        _joints = _bvh->GetJointList();

        int numJoints = _joints.size();
        _boneParents.resize(numJoints);
        _localPositions.resize(numJoints);
        _localRotations.resize(numJoints);
        _globalPositions.resize(numJoints);
        _globalRotations.resize(numJoints);

        // Resize skinning arrays to match character bones (23)
        int numSkinningBones = 23;
        _skinningPositions.resize(numSkinningBones);
        _skinningRotations.resize(numSkinningBones);

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

    void CaseBVHMotionMatching::UpdateFrame(float dt) {
        if (! _bvh) return;

        _currentTime += dt;
        float totalDuration = _bvh->frames() * _bvh->frame_time();
        if (_currentTime > totalDuration) {
            _currentTime = std::fmod(_currentTime, totalDuration);
        }

        _frameIndex = static_cast<int>(_currentTime / _bvh->frame_time());
        _frameIndex = std::clamp(_frameIndex, 0, _bvh->frames() - 1);

        // 1. Sample BVH
        for (int i = 0; i < _joints.size(); ++i) {
            auto               transform = _bvh->GetTransformationRelativeToParent(_joints[i], _frameIndex);
            Eigen::Vector3d    pos       = transform.translation();
            Eigen::Quaterniond rot(transform.rotation());

            _localPositions(i) = Core::Math::vec3(pos.x(), pos.y(), pos.z()) * 0.01f;
            _localRotations(i) = Core::Math::quat(rot.w(), rot.x(), rot.y(), rot.z());
        }

        // 2. FK for BVH
        Core::Animation::forward_kinematics_full(
            _globalPositions,
            _globalRotations,
            _localPositions,
            _localRotations,
            _boneParents);

        // 3. Retargeting / Injection
        // Map BVH joints to Skinning Bones
        // Assumption: BVH Root (0) is Hips (1)
        // BVH joints 0..21 map to Skinning Bones 1..22

        for (int i = 0; i < _joints.size(); ++i) {
            if (i + 1 < _skinningPositions.size) {
                _skinningPositions(i + 1) = _globalPositions(i);
                _skinningRotations(i + 1) = _globalRotations(i);
            }
        }

        // Calculate Simulation Bone (Index 0)
        // Needs Spine2 (Index 12 in Skinning, so Index 11 in BVH) and Hips (Index 1 in Skinning, Index 0 in BVH)
        if (_joints.size() > 11) {
            using namespace Core::Math;

            vec3 spine2Pos = _globalPositions(11); // Spine2
            vec3 hipsPos   = _globalPositions(0);  // Hips
            quat hipsRot   = _globalRotations(0);  // Hips

            // Sim Position: Project Spine2 to ground
            vec3 simPos = spine2Pos;
            simPos.y    = 0.0f; // Assuming Y is up

            // Sim Rotation: Project Hips forward to ground
            vec3 localForward(0, 1, 0); // Y is forward in bone local space
            vec3 worldForward = quat_mul_vec3(hipsRot, localForward);

            worldForward.y = 0.0f; // Project to XZ
            worldForward   = normalize(worldForward);

            // Compute rotation from Z axis [0,0,1] to worldForward
            vec3 targetZ = worldForward;
            vec3 sourceZ(0, 0, 1);

            quat simRot = quat_between(sourceZ, targetZ);

            _skinningPositions(0) = simPos;
            _skinningRotations(0) = simRot;
        } else {
            // Fallback
            _skinningPositions(0)   = _globalPositions(0);
            _skinningPositions(0).y = 0;
            _skinningRotations(0)   = _globalRotations(0);
        }

        // 4. Skinning
        Core::Animation::linear_blend_skinning_positions(
            _character.positions,
            _restPositions,
            _character.bone_weights,
            _character.bone_indices,
            _character.bone_rest_positions,
            _character.bone_rest_rotations,
            _skinningPositions,
            _skinningRotations);

        Core::Animation::linear_blend_skinning_normals(
            _character.normals,
            _restNormals,
            _character.bone_weights,
            _character.bone_indices,
            _character.bone_rest_rotations,
            _skinningRotations);

        // 5. Upload to GPU
        glBindBuffer(GL_ARRAY_BUFFER, _vboPos.Get());
        glBufferSubData(GL_ARRAY_BUFFER, 0, _character.positions.size * sizeof(Core::Math::vec3), _character.positions.data);

        glBindBuffer(GL_ARRAY_BUFFER, _vboNorm.Get());
        glBufferSubData(GL_ARRAY_BUFFER, 0, _character.normals.size * sizeof(Core::Math::vec3), _character.normals.data);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    void CaseBVHMotionMatching::OnSetupPropsUI() {
        const char * bvhNames[] = {
            "Aiming", "Dance", "FallAndGetUp", "Fight", "Ground", "MultipleActions", "Obstacles", "PushAndStumble", "Run", "Walk"
        };

        if (ImGui::Combo("BVH File", &_currentBVH, bvhNames, IM_ARRAYSIZE(bvhNames))) {
            _bvh = nullptr;
            _joints.clear();
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

    Common::CaseRenderResult CaseBVHMotionMatching::OnRender(std::pair<std::uint32_t, std::uint32_t> const desiredSize) {
        if (_loadFuture.valid() && _loadFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            _bvh = _loadFuture.get();
            OnBVHLoaded();
        }

        _frame.Resize(desiredSize, _enableMSAA ? 4 : 1);

        _cameraManager.Update(_camera);
        _program.GetUniforms().SetByName("mvp", _camera.GetProjectionMatrix((float(desiredSize.first) / desiredSize.second)) * _camera.GetViewMatrix());
        _program.GetUniforms().SetByName("matModel", glm::mat4(1.0f));
        _program.GetUniforms().SetByName("matNormal", glm::mat4(1.0f));
        _program.GetUniforms().SetByName("colDiffuse", glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

        gl_using(_frame);
        glEnable(GL_DEPTH_TEST);
        glClearColor(0.9f, 0.9f, 0.9f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (! _stopped) {
            UpdateFrame(ImGui::GetIO().DeltaTime);
        } else {
            // Still update to render current frame if paused
            UpdateFrame(0.0f);
        }

        gl_using(_program);
        glBindVertexArray(_vao.Get());
        glDrawElements(GL_TRIANGLES, _character.triangles.size, GL_UNSIGNED_SHORT, 0);
        glBindVertexArray(0);

        // Render Ground
        glm::mat4 mvp = _camera.GetProjectionMatrix((float(desiredSize.first) / desiredSize.second)) * _camera.GetViewMatrix();
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

    void CaseBVHMotionMatching::OnProcessInput(ImVec2 const & pos) {
        _cameraManager.ProcessInput(_camera, pos);
    }
} // namespace VCX::Labs::MotionMatching
