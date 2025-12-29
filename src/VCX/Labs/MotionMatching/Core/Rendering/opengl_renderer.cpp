#include "opengl_renderer.h"

#include <fstream>
#include <iostream>
#include <sstream>

#include "Engine/GL/Shader.h"

namespace VCX::Labs::MotionMatching::Core::Rendering {

    using namespace VCX::Engine::GL;

    OpenGLCharacterRenderer::OpenGLCharacterRenderer() = default;

    OpenGLCharacterRenderer::~OpenGLCharacterRenderer() = default;

    bool OpenGLCharacterRenderer::initialize() {
        // 初始化着色器
        if (! initShaders()) {
            std::cerr << "Failed to initialize shaders" << std::endl;
            return false;
        }

        // 创建蒙皮矩阵 UBO
        _skinningMatricesUBO = UniqueUniformBuffer();

        return true;
    }

    void OpenGLCharacterRenderer::setCharacterMesh(const SimpleMesh & mesh) {
        _mesh = mesh;

        // 计算边界框
        Core::Math::vec3 minBound, maxBound;
        compute_bounds(_mesh, minBound, maxBound);
        _bboxMin = glm::vec3(minBound.x, minBound.y, minBound.z);
        _bboxMax = glm::vec3(maxBound.x, maxBound.y, maxBound.z);

        // 创建顶点缓冲区
        if (! createVertexBuffers()) {
            std::cerr << "Failed to create vertex buffers" << std::endl;
        }
    }

    void OpenGLCharacterRenderer::updateSkinningMatrices(const std::vector<mat4> & skinningMatrices) {
        _skinningMatrices = skinningMatrices;

        // 更新 UBO
        if (_skinningMatricesUBO.Get() != 0 && ! _skinningMatrices.empty()) {
            auto useUBO = _skinningMatricesUBO.Use();
            glBufferData(GL_UNIFORM_BUFFER, _skinningMatrices.size() * sizeof(mat4), _skinningMatrices.data(), GL_DYNAMIC_DRAW);
        }
    }

    void OpenGLCharacterRenderer::render(const mat4 & viewMatrix, const mat4 & projectionMatrix, const vec3 & lightPosition, const vec3 & cameraPosition) {
        if (! _characterProgram || _vertexCount == 0) {
            return;
        }

        // 使用着色器程序
        auto useProgram = _characterProgram->Use();

        // 设置 MVP 矩阵
        mat4 modelMatrix  = glm::mat4(1.0f);
        mat4 mvp          = projectionMatrix * viewMatrix * modelMatrix;
        mat4 normalMatrix = glm::transpose(glm::inverse(viewMatrix * modelMatrix));

        auto & uniforms = _characterProgram->GetUniforms();
        uniforms.SetByName("mvp", mvp);
        uniforms.SetByName("matModel", modelMatrix);
        uniforms.SetByName("matNormal", normalMatrix);
        uniforms.SetByName("lightPosition", lightPosition);
        uniforms.SetByName("cameraPosition", cameraPosition);

        // 绑定蒙皮矩阵 UBO
        if (_skinningMatricesUBO.Get() != 0) {
            glBindBufferBase(GL_UNIFORM_BUFFER, 0, _skinningMatricesUBO.Get());
        }

        // 渲染网格
        auto useVAO = _characterVAO.Use();
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(_indexCount), GL_UNSIGNED_INT, nullptr);
    }

    void OpenGLCharacterRenderer::getBoundingBox(vec3 & min, vec3 & max) const {
        min = _bboxMin;
        max = _bboxMax;
    }

    bool OpenGLCharacterRenderer::initShaders() {
        try {
            // 加载顶点着色器
            std::filesystem::path vertPath = "assets/motion-matching/shaders/character.vert";
            SharedShader          vertShader(ShaderType::Vertex, vertPath);

            // 加载片段着色器
            std::filesystem::path fragPath = "assets/motion-matching/shaders/character.frag";
            SharedShader          fragShader(ShaderType::Fragment, fragPath);

            // 创建着色器程序
            _characterProgram = std::make_unique<UniqueProgram>(
                std::initializer_list<SharedShader> { vertShader, fragShader });

            return true;
        } catch (const std::exception & e) {
            std::cerr << "Shader initialization failed: " << e.what() << std::endl;
            return false;
        }
    }

    bool OpenGLCharacterRenderer::createVertexBuffers() {
        if (_mesh.positions.empty() || _mesh.indices.empty()) {
            return false;
        }

        // 创建顶点数组对象
        _characterVAO = UniqueVertexArray();
        auto useVAO   = _characterVAO.Use();

        // 创建顶点缓冲区
        _characterVBO = UniqueArrayBuffer();
        auto useVBO   = _characterVBO.Use();

        // 上传顶点数据
        std::vector<float> vertexData;
        vertexData.reserve(_mesh.positions.size() * 8); // position(3) + normal(3) + texcoord(2)

        for (size_t i = 0; i < _mesh.positions.size(); ++i) {
            // 位置
            vertexData.push_back(_mesh.positions[i].x);
            vertexData.push_back(_mesh.positions[i].y);
            vertexData.push_back(_mesh.positions[i].z);

            // 法线
            if (i < _mesh.normals.size()) {
                vertexData.push_back(_mesh.normals[i].x);
                vertexData.push_back(_mesh.normals[i].y);
                vertexData.push_back(_mesh.normals[i].z);
            } else {
                vertexData.push_back(0.0f);
                vertexData.push_back(1.0f);
                vertexData.push_back(0.0f);
            }

            // 纹理坐标
            if (i < _mesh.texcoords.size()) {
                vertexData.push_back(_mesh.texcoords[i].x);
                vertexData.push_back(_mesh.texcoords[i].y);
            } else {
                vertexData.push_back(0.0f);
                vertexData.push_back(0.0f);
            }
        }

        glBufferData(GL_ARRAY_BUFFER, vertexData.size() * sizeof(float), vertexData.data(), GL_STATIC_DRAW);

        // 设置顶点属性
        // 位置属性
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *) 0);

        // 法线属性
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *) (3 * sizeof(float)));

        // 纹理坐标属性
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *) (6 * sizeof(float)));

        // 创建索引缓冲区
        _characterEBO = UniqueElementArrayBuffer();
        auto useEBO   = _characterEBO.Use();

        glBufferData(GL_ELEMENT_ARRAY_BUFFER, _mesh.indices.size() * sizeof(unsigned int), _mesh.indices.data(), GL_STATIC_DRAW);

        _vertexCount = _mesh.positions.size();
        _indexCount  = _mesh.indices.size();

        return true;
    }
} // namespace VCX::Labs::MotionMatching::Core::Rendering
