#include "Labs/MotionMatching/SceneEnvironment.hpp"
#include "Assets/bundled.h"
#include <glm/gtc/constants.hpp>

namespace VCX::Labs::MotionMatching {
    SceneEnvironment::SceneEnvironment():
        _programGround(
            Engine::GL::UniqueProgram({ Engine::GL::SharedShader("assets/shaders/checkerboard.vert"), Engine::GL::SharedShader("assets/shaders/checkerboard.frag") })),
        _programFlat(
            Engine::GL::UniqueProgram({ Engine::GL::SharedShader("assets/shaders/flat.vert"), Engine::GL::SharedShader("assets/shaders/flat.frag") })) {
        // Ground
        std::vector<Vertex> groundVertices = {
            { { -100.0f, 0.0f, -100.0f }, { 0.0f, 1.0f, 0.0f } },
            {  { 100.0f, 0.0f, -100.0f }, { 0.0f, 1.0f, 0.0f } },
            {   { 100.0f, 0.0f, 100.0f }, { 0.0f, 1.0f, 0.0f } },
            { { -100.0f, 0.0f, -100.0f }, { 0.0f, 1.0f, 0.0f } },
            {   { 100.0f, 0.0f, 100.0f }, { 0.0f, 1.0f, 0.0f } },
            {  { -100.0f, 0.0f, 100.0f }, { 0.0f, 1.0f, 0.0f } },
        };

        glBindVertexArray(_vaoGround.Get());
        glBindBuffer(GL_ARRAY_BUFFER, _vboGround.Get());
        glBufferData(GL_ARRAY_BUFFER, groundVertices.size() * sizeof(Vertex), groundVertices.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *) 0);

        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *) offsetof(Vertex, Normal));

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        // Axis & Dot
        std::vector<VertexColor> axisVertices;

        AddBox(axisVertices, glm::vec3(0.45f, 0.0f, 0.0f), glm::vec3(0.9f, 0.02f, 0.02f), glm::vec3(1, 0, 0));
        AddCone(axisVertices, glm::vec3(0.9f, 0.0f, 0.0f), 0.04f, 0.1f, glm::vec3(1, 0, 0), glm::vec3(1, 0, 0));

        AddBox(axisVertices, glm::vec3(0.0f, 0.45f, 0.0f), glm::vec3(0.02f, 0.9f, 0.02f), glm::vec3(0, 1, 0));
        AddCone(axisVertices, glm::vec3(0.0f, 0.9f, 0.0f), 0.04f, 0.1f, glm::vec3(0, 1, 0), glm::vec3(0, 1, 0));

        AddBox(axisVertices, glm::vec3(0.0f, 0.0f, 0.45f), glm::vec3(0.02f, 0.02f, 0.9f), glm::vec3(0, 0, 1));
        AddCone(axisVertices, glm::vec3(0.0f, 0.0f, 0.9f), 0.04f, 0.1f, glm::vec3(0, 0, 1), glm::vec3(0, 0, 1));

        AddSphere(axisVertices, glm::vec3(0, 0, 0), 0.05f, glm::vec3(0, 0, 0));

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

    void SceneEnvironment::Render(glm::mat4 const & mvp, bool showAxis) {
        // Render Ground
        _programGround.GetUniforms().SetByName("mvp", mvp);
        _programGround.GetUniforms().SetByName("matModel", glm::mat4(1.0f));
        _programGround.GetUniforms().SetByName("matNormal", glm::transpose(glm::inverse(glm::mat4(1.0f))));

        gl_using(_programGround);
        glBindVertexArray(_vaoGround.Get());
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);

        // Render Axis & Dot
        if (showAxis) {
            glDisable(GL_DEPTH_TEST);
            _programFlat.GetUniforms().SetByName("u_MVP", mvp);
            gl_using(_programFlat);
            glBindVertexArray(_vaoAxis.Get());
            glDrawArrays(GL_TRIANGLES, 0, _axisVertexCount);
            glBindVertexArray(0);
            glEnable(GL_DEPTH_TEST);
        }
    }
} // namespace VCX::Labs::MotionMatching