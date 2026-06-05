#version 330 core

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTex;

out vec2 TexCoord;

uniform vec2 screenSize;

void main()
{
    vec2 pos = aPos / screenSize * 2.0 - 1.0;
    pos.y = -pos.y;
    gl_Position = vec4(pos, 0.0, 1.0);
    TexCoord = aTex;
}