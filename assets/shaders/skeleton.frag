#version 410 core

in vec3 v_Normal;
in vec3 v_FragPos;

out vec4 FragColor;

uniform vec3 u_Color;
uniform vec3 u_LightDir;

void main() {
    vec3 norm = normalize(v_Normal);
    vec3 lightDir = normalize(u_LightDir);
    float diff = max(dot(norm, lightDir), 0.0);

    vec3 diffuse = diff * u_Color;
    vec3 ambient = 0.3 * u_Color;

    FragColor = vec4(ambient + diffuse, 1.0);
}
