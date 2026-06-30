#version 460 core

in vec2 TexCoord;
flat in int TexLayer;

uniform sampler2DArray atlas;

void main()
{
    vec4 texColor = texture(atlas, vec3(fract(TexCoord), TexLayer));
    
    // Only write depth if alpha is mostly opaque
    if (texColor.a < 0.5)
        discard;
}
