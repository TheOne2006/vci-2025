#include "Labs/MotionMatching/Utils.hpp"
#include "Assets/bundled.h"
#include <glm/gtc/constants.hpp>

namespace VCX::Labs::MotionMatching {

    void AddBox(std::vector<VertexColor> & vertices, glm::vec3 center, glm::vec3 size, glm::vec3 color) {
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
            vertices.push_back({ p[a], color });
            vertices.push_back({ p[b], color });
            vertices.push_back({ p[c], color });
            vertices.push_back({ p[c], color });
            vertices.push_back({ p[d], color });
            vertices.push_back({ p[a], color });
        };
        addQuad(0, 1, 2, 3);
        addQuad(5, 4, 7, 6);
        addQuad(4, 0, 3, 7);
        addQuad(1, 5, 6, 2);
        addQuad(3, 2, 6, 7);
        addQuad(4, 5, 1, 0);
    }

    void AddCone(std::vector<VertexColor> & vertices, glm::vec3 baseCenter, float radius, float height, glm::vec3 axis, glm::vec3 color) {
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

            vertices.push_back({ p1, color });
            vertices.push_back({ p2, color });
            vertices.push_back({ tip, color });
            vertices.push_back({ baseCenter, color });
            vertices.push_back({ p2, color });
            vertices.push_back({ p1, color });
        }
    }

    void AddSphere(std::vector<VertexColor> & vertices, glm::vec3 center, float radius, glm::vec3 color) {
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
                vertices.push_back({ p1, color });
                vertices.push_back({ p2, color });
                vertices.push_back({ p3, color });
                vertices.push_back({ p3, color });
                vertices.push_back({ p4, color });
                vertices.push_back({ p1, color });
            }
        }
    }
} // namespace VCX::Labs::MotionMatching
