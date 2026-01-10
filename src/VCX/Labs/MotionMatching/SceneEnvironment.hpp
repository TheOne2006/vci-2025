#pragma once

#include "Engine/GL/Program.h"
#include "Engine/GL/RenderItem.h"
#include "Engine/GL/resource.hpp"
#include "Utils.hpp"
#include <glm/glm.hpp>
#include <vector>

namespace VCX::Labs::MotionMatching {
    class SceneEnvironment {
    public:
        SceneEnvironment();

        // Exposed parameters
        float GroundHeight = -0.01f;

        void Render(glm::mat4 const & mvp, bool showAxis = true);

    private:
        Engine::GL::UniqueProgram     _programGround;
        Engine::GL::UniqueProgram     _programFlat;
        Engine::GL::UniqueVertexArray _vaoGround;
        Engine::GL::UniqueArrayBuffer _vboGround;
        Engine::GL::UniqueVertexArray _vaoAxis;
        Engine::GL::UniqueArrayBuffer _vboAxis;
        std::size_t                   _axisVertexCount { 0 };
    };
}