#version 460 core

layout(location=0) in vec3 aPos;
layout(location=1) in vec2 aUV;
layout(location=2) in float aLayer;
layout(location=3) in float aLight;

out vec2 TexCoord;
flat out int TexLayer;
out vec3 FragPos;
out float FaceLight;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    TexCoord = aUV;

    TexLayer = int(aLayer);

    FaceLight = aLight;

    vec3 worldPos =
        vec3(model * vec4(aPos, 1.0));

    FragPos = worldPos;

    gl_Position =
        projection *
        view *
        vec4(worldPos, 1.0);
}