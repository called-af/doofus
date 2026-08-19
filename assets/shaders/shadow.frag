#version 460 core

in vec2 TexCoord;

uniform sampler2D tex;
uniform int uUseTexture;

void main()
{
    if (uUseTexture == 1) {
        vec4 c = texture(tex, TexCoord);
        if (c.a < 0.2) discard;
    }
}
