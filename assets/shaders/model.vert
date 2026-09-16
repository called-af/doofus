#version 460 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec2 TexCoord;
out vec3 FragPos;

void main()
{
    TexCoord = aUV;

    vec4 worldPos = model * vec4(aPos, 1.0);
    FragPos = vec3(worldPos);

    gl_Position = projection * view * worldPos;
}
