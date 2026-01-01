#version 410 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in mat4 i_ModelMatrix;

uniform mat4 u_Projection;
uniform mat4 u_View;

out vec3 v_Normal;
out vec3 v_FragPos;

void main() {
    vec4 worldPos = i_ModelMatrix * vec4(a_Position, 1.0);
    v_FragPos = worldPos.xyz;
    v_Normal = mat3(i_ModelMatrix) * a_Normal;
    gl_Position = u_Projection * u_View * worldPos;
}
