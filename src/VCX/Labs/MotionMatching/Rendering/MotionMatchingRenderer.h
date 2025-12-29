#pragma once

#include <memory>
#include <vector>

#include <glm/glm.hpp>

#include "Engine/Camera.hpp"
#include "Engine/GL/Frame.hpp"
#include "Engine/GL/Program.h"
#include "Engine/GL/RenderItem.h"
#include "Labs/Common/OrbitCameraManager.h"

#include "../Core/Animation/character.h"
#include "../Core/MotionMatching/database.h"
#include "../Core/Rendering/mesh_processor.h"

namespace VCX::Labs::MotionMatching {

    // 运动匹配渲染器 - 使用 VCX 框架的 GL 抽象
    class MotionMatchingRenderer {
    public:
        MotionMatchingRenderer();

        // 初始化渲染器
        bool Initialize();

        // 设置角色数据
        void SetCharacterData(
            const ::VCX::Labs::MotionMatching::Core::Animation::character &     character,
            const ::VCX::Labs::MotionMatching::Core::MotionMatching::database & database);

        // 更新动画状态
        void UpdateAnimationState(
            int                            currentFrame,
            const std::vector<glm::mat4> & skinningMatrices);

        // 渲染角色
        void RenderCharacter(
            Engine::GL::UniqueProgram & program,
            const Engine::Camera &      camera,
            const glm::vec3 &           lightPosition);

        // 渲染地面
        void RenderGround(
            Engine::GL::UniqueProgram & program,
            const Engine::Camera &      camera);

        // 获取边界框
        void GetBoundingBox(glm::vec3 & min, glm::vec3 & max) const;

        // 获取角色网格
        const ::VCX::Labs::MotionMatching::Core::Rendering::SimpleMesh & GetCharacterMesh() const { return _characterMesh; }

        // 获取角色渲染项
        Engine::GL::UniqueIndexedRenderItem & GetCharacterRenderItem() { return _characterItem; }

        // 获取地面渲染项
        Engine::GL::UniqueIndexedRenderItem & GetGroundRenderItem() { return _groundItem; }

    private:
        // 角色网格数据
        ::VCX::Labs::MotionMatching::Core::Rendering::SimpleMesh _characterMesh;

        // 地面网格数据
        ::VCX::Labs::MotionMatching::Core::Rendering::SimpleMesh _groundMesh;

        // 渲染项
        Engine::GL::UniqueIndexedRenderItem _characterItem;
        Engine::GL::UniqueIndexedRenderItem _groundItem;

        // 蒙皮矩阵
        std::vector<glm::mat4> _skinningMatrices;

        // 边界框
        glm::vec3 _bboxMin = glm::vec3(0.0f, 0.0f, 0.0f);
        glm::vec3 _bboxMax = glm::vec3(0.0f, 0.0f, 0.0f);

        // 创建角色网格
        bool CreateCharacterMesh(
            const ::VCX::Labs::MotionMatching::Core::Animation::character &     character,
            const ::VCX::Labs::MotionMatching::Core::MotionMatching::database & database);

        // 创建地面网格
        bool CreateGroundMesh();

        // 更新角色顶点缓冲区
        void UpdateCharacterVertexBuffer();
    };
} // namespace VCX::Labs::MotionMatching
