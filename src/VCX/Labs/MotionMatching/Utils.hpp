#pragma once

#include "Engine/GL/Program.h"
#include "Engine/GL/RenderItem.h"
#include "Engine/GL/resource.hpp"
#include <glm/glm.hpp>
#include <vector>

namespace VCX::Labs::MotionMatching {

    struct Vertex {
        glm::vec3 Position;
        glm::vec3 Normal;
    };

    struct VertexColor {
        glm::vec3 Position;
        glm::vec3 Color;
    };

    void AddBox(std::vector<VertexColor> & vertices, glm::vec3 center, glm::vec3 size, glm::vec3 color);
    void AddCone(std::vector<VertexColor> & vertices, glm::vec3 baseCenter, float radius, float height, glm::vec3 axis, glm::vec3 color);
    void AddSphere(std::vector<VertexColor> & vertices, glm::vec3 center, float radius, glm::vec3 color);
    void AddLine(std::vector<VertexColor> & vertices, glm::vec3 start, glm::vec3 end, glm::vec3 color);
    void AddCircle(std::vector<VertexColor> & vertices, glm::vec3 center, float radius, glm::vec3 normal, glm::vec3 color);
} // namespace VCX::Labs::MotionMatching
