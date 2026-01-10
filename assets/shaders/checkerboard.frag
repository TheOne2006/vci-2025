#version 410 core
precision mediump float;

in vec3 fragPosition;
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;

uniform vec4 colDiffuse;

out vec4 finalColor;

void main()
{
    // Compute texture coordinates
    vec2 uv = fragPosition.xz * 2.0f;

    // Compute filter width based on screen-space derivatives
    vec2 w = fwidth(uv) + 0.001f;

    // Analytical integral (box filter) for antialiasing
    // This calculates the coverage of the checkerboard pattern within the pixel footprint
    vec2 i = 2.0f * (abs(fract((uv - 0.5f * w) * 0.5f) - 0.5f) - abs(fract((uv + 0.5f * w) * 0.5f) - 0.5f)) / w;
    float intensity = 0.5f - 0.5f * i.x * i.y;

    // Interpolate between the two checker colors based on the calculated intensity
    finalColor = mix(vec4(0.85f, 0.85f, 0.85f, 1.0f), vec4(0.8f, 0.8f, 0.8f, 1.0f), intensity);
}
