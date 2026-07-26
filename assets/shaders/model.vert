#version 460 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightSpaceMatrix;

uniform vec3 uLightDir;

out vec2 TexCoord;
out vec3 FragPos;
out vec3 Normal;
out vec4 FragPosLightSpace;
out float vNdotL;

void main()
{
    TexCoord = aUV;

    vec4 worldPos = model * vec4(aPos, 1.0);
    FragPos = vec3(worldPos);
    FragPosLightSpace = lightSpaceMatrix * worldPos;

    Normal = vec3(0.0, 1.0, 0.0);
    vNdotL = max(dot(Normal, uLightDir), 0.35);

    gl_Position = projection * view * worldPos;
}