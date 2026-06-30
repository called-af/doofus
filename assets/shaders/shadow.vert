#version 460 core

layout(location=0) in vec3 aPos;
layout(location=1) in vec2 aUV;
layout(location=2) in float aLayer;
layout(location=3) in float aLight;

out vec2 TexCoord;
flat out int TexLayer;

uniform mat4 model;
uniform mat4 lightSpaceMatrix;

void main()
{
    TexCoord = aUV;
    TexLayer = int(aLayer);
    
    vec3 worldPos = vec3(model * vec4(aPos, 1.0));
    gl_Position = lightSpaceMatrix * vec4(worldPos, 1.0);
}
