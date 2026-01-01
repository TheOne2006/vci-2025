#pragma once

#include "Engine/GL/Program.h"
#include "Engine/GL/RenderItem.h"
#include "Engine/GL/resource.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace VCX::Labs::MotionMatching {
    class SimulationObject {
    public:
        SimulationObject();

        void Render(
            glm::mat4 const &              view,
            glm::mat4 const &              proj,
            glm::vec3 const &              position,
            glm::quat const &              rotation,
            std::vector<glm::vec3> const & trajectoryPositions,
            std::vector<glm::quat> const & trajectoryRotations,
            glm::vec3 const &              desiredDirection = glm::vec3(0.0f));

    private:
        Engine::GL::UniqueProgram     _program;
        Engine::GL::UniqueVertexArray _vao;
        Engine::GL::UniqueArrayBuffer _vbo;
    };
} // namespace VCX::Labs::MotionMatching
