#version 330 core

out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D tex;
uniform int   mode;
uniform vec3  textColor;

void main()
{
    if (mode == 1)
    {
        float alpha = texture(tex, TexCoord).r;
        if (alpha < 0.01) discard;
        FragColor = vec4(textColor, alpha);
    }
    else
    {
        vec4 color = texture(tex, TexCoord);
        if (color.a < 0.1) discard;
        FragColor = color;
    }
}