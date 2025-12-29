#pragma once

#include <memory>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Engine/GL/Program.h"
#include "Engine/GL/resource.hpp"

#include "mesh_processor.h"

namespace VCX::Labs::MotionMatching::Core::Rendering {

    using namespace Math;
    using namespace VCX::Engine::GL;

    // 使用 glm 类型别名
    using glm::mat3;
    using glm::mat4;
    using glm::vec2;
    using glm::vec3;
    using glm::vec4;

    // 角色渲染器（OpenGL 实现）
    class OpenGLCharacterRenderer {
    public:
        OpenGLCharacterRenderer();
        ~OpenGLCharacterRenderer();

        // 初始化渲染器
        bool initialize();

        // 设置角色网格
        void setCharacterMesh(const SimpleMesh & mesh);

        // 更新蒙皮矩阵
        void updateSkinningMatrices(const std::vector<mat4> & skinningMatrices);

        // 渲染角色
        void render(const mat4 & viewMatrix, const mat4 & projectionMatrix, const vec3 & lightPosition, const vec3 & cameraPosition);

        // 获取边界框
        void getBoundingBox(vec3 & min, vec3 & max) const;

    private:
        // 着色器程序
        std::unique_ptr<UniqueProgram> _characterProgram;

        // 顶点缓冲区
        UniqueVertexArray        _characterVAO;
        UniqueArrayBuffer        _characterVBO;
        UniqueElementArrayBuffer _characterEBO;

        // 蒙皮矩阵缓冲区
        UniqueUniformBuffer _skinningMatricesUBO;
        std::vector<mat4>   _skinningMatrices;

        // 网格数据
        SimpleMesh _mesh;

        // 顶点计数
        std::size_t _vertexCount = 0;
        std::size_t _indexCount  = 0;

        // 边界框
        vec3 _bboxMin = vec3(0.0f, 0.0f, 0.0f);
        vec3 _bboxMax = vec3(0.0f, 0.0f, 0.0f);

        // 初始化着色器
        bool initShaders();

        // 创建顶点缓冲区
        bool createVertexBuffers();
    };
} // namespace VCX::Labs::MotionMatching::Core::Rendering
