#version 460 core

in vec2 TexCoord;
flat in int TexLayer;
in vec3 FragPos;
in float FaceLight;

out vec4 FragColor;

uniform sampler2DArray atlas;

uniform vec3 cameraPos;
uniform vec3 fogColor;

uniform float fogStart;
uniform float fogEnd;

void main()
{
    vec4 texColor =
        texture(
            atlas,
            vec3(fract(TexCoord), TexLayer)
        );

    texColor.rgb *= FaceLight;

    float dist =
        length(FragPos - cameraPos);

    float fog =
        clamp(
            (fogEnd - dist) /
            (fogEnd - fogStart),
            0.0,
            1.0
        );

    FragColor =
        mix(
            vec4(fogColor, 1.0),
            texColor,
            fog
        );
}