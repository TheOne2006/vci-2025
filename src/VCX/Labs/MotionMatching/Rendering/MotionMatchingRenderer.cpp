#include "MotionMatchingRenderer.h"

namespace VCX::Labs::MotionMatching::Rendering {

    using namespace Engine;
    using namespace Engine::GL;

    MotionMatchingRenderer::MotionMatchingRenderer():
        _characterItem(
            VertexLayout()
                .Add<glm::vec3>("position", DrawFrequency::Stream, 0)
                .Add<glm::vec3>("normal", DrawFrequency::Stream, 0),
            PrimitiveType::Triangles),
        _groundItem(
            VertexLayout()
                .Add<glm::vec3>("position", DrawFrequency::Static, 0)
                .Add<glm::vec3>("normal", DrawFrequency::Static, 0),
            PrimitiveType::Triangles) {
    }

    bool MotionMatchingRenderer::Initialize() {
        // 创建地面网格
        if (! CreateGroundMesh()) {
            return false;
        }

        // 更新地面顶点缓冲区（需要转换类型）
        std::vector<glm::vec3> groundPositions(_groundMesh.positions.size());
        std::vector<glm::vec3> groundNormals(_groundMesh.normals.size());

        for (size_t i = 0; i < _groundMesh.positions.size(); ++i) {
            groundPositions[i] = glm::vec3(
                _groundMesh.positions[i].x,
                _groundMesh.positions[i].y,
                _groundMesh.positions[i].z);
            groundNormals[i] = glm::vec3(
                _groundMesh.normals[i].x,
                _groundMesh.normals[i].y,
                _groundMesh.normals[i].z);
        }

        _groundItem.UpdateVertexBuffer("position", Engine::make_span_bytes<glm::vec3>(groundPositions));
        _groundItem.UpdateVertexBuffer("normal", Engine::make_span_bytes<glm::vec3>(groundNormals));
        _groundItem.UpdateElementBuffer(_groundMesh.indices);

        return true;
    }

    void MotionMatchingRenderer::SetCharacterData(
        const VCX::Labs::MotionMatching::Core::Animation::character & character,
        const VCX::Labs::MotionMatching::Core::Animation::database &  database) {
        // 创建角色网格
        if (CreateCharacterMesh(character, database)) {
            // 更新角色顶点缓冲区
            UpdateCharacterVertexBuffer();
        }
    }

    void MotionMatchingRenderer::UpdateAnimationState(
        int                            currentFrame,
        const std::vector<glm::mat4> & skinningMatrices) {
        _skinningMatrices = skinningMatrices;

        // 应用蒙皮变换到角色网格
        if (! _characterMesh.positions.empty() && ! _skinningMatrices.empty()) {
            // 这里应该实现蒙皮变换，但为了简化，我们暂时只更新边界框
            // 实际实现中应该调用 Core::Rendering::apply_skinning 函数
        }

        // 更新角色顶点缓冲区
        UpdateCharacterVertexBuffer();
    }

    void MotionMatchingRenderer::RenderCharacter(
        Engine::GL::UniqueProgram & program,
        const Engine::Camera &      camera,
        const glm::vec3 &           lightPosition) {
        // 设置着色器 uniform
        program.GetUniforms().SetByName("u_Projection", camera.GetProjectionMatrix(1.0f));
        program.GetUniforms().SetByName("u_View", camera.GetViewMatrix());
        program.GetUniforms().SetByName("u_Model", glm::mat4(1.0f));
        program.GetUniforms().SetByName("u_LightPosition", lightPosition);
        program.GetUniforms().SetByName("u_CameraPosition", camera.Eye);

        // 渲染角色
        _characterItem.Draw({ program.Use() });
    }

    void MotionMatchingRenderer::RenderGround(
        Engine::GL::UniqueProgram & program,
        const Engine::Camera &      camera) {
        // 设置着色器 uniform
        program.GetUniforms().SetByName("u_Projection", camera.GetProjectionMatrix(1.0f));
        program.GetUniforms().SetByName("u_View", camera.GetViewMatrix());
        program.GetUniforms().SetByName("u_Model", glm::mat4(1.0f));

        // 渲染地面
        _groundItem.Draw({ program.Use() });
    }

    void MotionMatchingRenderer::GetBoundingBox(glm::vec3 & min, glm::vec3 & max) const {
        min = _bboxMin;
        max = _bboxMax;
    }

    bool MotionMatchingRenderer::CreateCharacterMesh(
        const ::VCX::Labs::MotionMatching::Core::Animation::character & character,
        const ::VCX::Labs::MotionMatching::Core::Animation::database &  database) {
        // 使用 mesh_processor 创建简单网格
        _characterMesh = ::VCX::Labs::MotionMatching::Core::Rendering::create_simple_mesh(character);

        // 计算边界框
        ::VCX::Labs::MotionMatching::Core::Math::vec3 minBound, maxBound;
        ::VCX::Labs::MotionMatching::Core::Rendering::compute_bounds(
            _characterMesh,
            minBound,
            maxBound);

        // 转换为 glm 类型
        _bboxMin = glm::vec3(minBound.x, minBound.y, minBound.z);
        _bboxMax = glm::vec3(maxBound.x, maxBound.y, maxBound.z);

        // 更新索引缓冲区
        _characterItem.UpdateElementBuffer(_characterMesh.indices);

        return true;
    }

    bool MotionMatchingRenderer::CreateGroundMesh() {
        // 创建简单的棋盘地面
        const float groundSize = 10.0f;
        const int   gridSize   = 20;

        // 生成顶点（使用 Core::Math::vec3 类型）
        for (int i = 0; i <= gridSize; ++i) {
            for (int j = 0; j <= gridSize; ++j) {
                float x = (i / float(gridSize) - 0.5f) * groundSize;
                float z = (j / float(gridSize) - 0.5f) * groundSize;
                _groundMesh.positions.emplace_back(
                    ::VCX::Labs::MotionMatching::Core::Math::vec3(x, 0.0f, z));
                _groundMesh.normals.emplace_back(
                    ::VCX::Labs::MotionMatching::Core::Math::vec3(0.0f, 1.0f, 0.0f));
            }
        }

        // 生成三角形
        for (int i = 0; i < gridSize; ++i) {
            for (int j = 0; j < gridSize; ++j) {
                int idx0 = i * (gridSize + 1) + j;
                int idx1 = idx0 + 1;
                int idx2 = idx0 + (gridSize + 1);
                int idx3 = idx2 + 1;

                _groundMesh.indices.push_back(idx0);
                _groundMesh.indices.push_back(idx2);
                _groundMesh.indices.push_back(idx1);

                _groundMesh.indices.push_back(idx1);
                _groundMesh.indices.push_back(idx2);
                _groundMesh.indices.push_back(idx3);
            }
        }

        return true;
    }

    void MotionMatchingRenderer::UpdateCharacterVertexBuffer() {
        if (! _characterMesh.positions.empty()) {
            // 转换 vec3 到 glm::vec3
            std::vector<glm::vec3> positions(_characterMesh.positions.size());
            std::vector<glm::vec3> normals(_characterMesh.normals.size());

            for (size_t i = 0; i < _characterMesh.positions.size(); ++i) {
                positions[i] = glm::vec3(_characterMesh.positions[i].x, _characterMesh.positions[i].y, _characterMesh.positions[i].z);
                normals[i]   = glm::vec3(_characterMesh.normals[i].x, _characterMesh.normals[i].y, _characterMesh.normals[i].z);
            }

            _characterItem.UpdateVertexBuffer("position", Engine::make_span_bytes<glm::vec3>(positions));
            _characterItem.UpdateVertexBuffer("normal", Engine::make_span_bytes<glm::vec3>(normals));
        }
    }
} // namespace VCX::Labs::MotionMatching::Rendering
