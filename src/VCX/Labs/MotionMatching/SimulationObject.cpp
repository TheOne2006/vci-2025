#include "Labs/MotionMatching/SimulationObject.hpp"
#include "Assets/bundled.h"
#include "Labs/MotionMatching/Utils.hpp"
#include <glm/gtc/matrix_transform.hpp>

namespace VCX::Labs::MotionMatching {
    SimulationObject::SimulationObject():
        _program(
            Engine::GL::UniqueProgram({ Engine::GL::SharedShader("assets/shaders/flat.vert"), Engine::GL::SharedShader("assets/shaders/flat.frag") })) {
        glBindVertexArray(_vao.Get());
        glBindBuffer(GL_ARRAY_BUFFER, _vbo.Get());

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VertexColor), (void *) 0);

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(VertexColor), (void *) offsetof(VertexColor, Color));

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    void SimulationObject::Render(
        glm::mat4 const &              view,
        glm::mat4 const &              proj,
        glm::vec3 const &              position,
        glm::quat const &              rotation,
        std::vector<glm::vec3> const & trajectoryPositions,
        std::vector<glm::quat> const & trajectoryRotations,
        glm::vec3 const &              desiredDirection) {
        glDisable(GL_DEPTH_TEST);

        std::vector<VertexColor> vertices;
        glm::vec3                orange(1.0f, 0.64f, 0.0f);
        glm::vec3                green(0.0f, 1.0f, 0.0f);

        // 1. Add Main Object (Transformed to World Space)
        {
            // Position Ring (Spring Ring)
            // Radius 0.6f, Orange
            // Normal (0,1,0) rotated by rotation
            AddCircle(vertices, position, 0.6f, rotation * glm::vec3(0, 1, 0), orange);

            // Center Point (Wireframe Sphere)
            // Radius 0.05f, Orange
            AddCircle(vertices, position, 0.05f, rotation * glm::vec3(1, 0, 0), orange);
            AddCircle(vertices, position, 0.05f, rotation * glm::vec3(0, 1, 0), orange);
            AddCircle(vertices, position, 0.05f, rotation * glm::vec3(0, 0, 1), orange);

            // Direction Arrow
            // Length 0.6f along Z axis, Orange
            glm::vec3 end = position + rotation * glm::vec3(0.0f, 0.0f, 0.6f);
            AddLine(vertices, position, end, orange);

            // Desired Direction Arrow (Green)
            if (glm::length(desiredDirection) > 0.001f) {
                glm::vec3 dirEnd = position + glm::normalize(desiredDirection) * 1.0f;
                AddLine(vertices, position, dirEnd, green);
            }
        }

        // 2. Add Trajectory
        if (! trajectoryPositions.empty()) {
            for (size_t i = 0; i < trajectoryPositions.size() && i < 4; ++i) {
                glm::vec3 pos = trajectoryPositions[i];

                // Wireframe Sphere (Radius 0.05f)
                AddCircle(vertices, pos, 0.05f, glm::vec3(1, 0, 0), orange);
                AddCircle(vertices, pos, 0.05f, glm::vec3(0, 1, 0), orange);
                AddCircle(vertices, pos, 0.05f, glm::vec3(0, 0, 1), orange);

                // Connection Line
                if (i > 0) {
                    AddLine(vertices, trajectoryPositions[i - 1], pos, orange);
                }

                // Direction Arrow (Length 0.6f)
                if (i < trajectoryRotations.size()) {
                    glm::vec3 dir = trajectoryRotations[i] * glm::vec3(0, 0, 1);
                    AddLine(vertices, pos, pos + dir * 0.6f, orange);
                }
            }
        }

        // 3. Upload and Draw
        if (! vertices.empty()) {
            glBindVertexArray(_vao.Get());
            glBindBuffer(GL_ARRAY_BUFFER, _vbo.Get());
            glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(VertexColor), vertices.data(), GL_DYNAMIC_DRAW);

            glm::mat4 mvp = proj * view; // Identity model matrix
            _program.GetUniforms().SetByName("u_MVP", mvp);

            gl_using(_program);
            glDrawArrays(GL_LINES, 0, vertices.size());
            glBindVertexArray(0);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }

        glEnable(GL_DEPTH_TEST);
    }
} // namespace VCX::Labs::MotionMatching
