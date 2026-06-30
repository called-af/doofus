#version 460 core

layout(location=0) in vec3 aPos;
layout(location=1) in vec2 aUV;
layout(location=2) in float aLayer;
layout(location=3) in float aLight;

out vec2  TexCoord;
flat out int   TexLayer;
out vec3  FragPos;
out float FaceLight;
out vec3  Normal;
out vec4  FragPosLightSpace;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightSpaceMatrix;

// Face index packed into aLayer high bits: 0=top,1=bottom,2=+X,3=-X,4=+Z,5=-Z
// For voxels we derive the normal from the face light bake, but we also
// store it explicitly so the frag shader can do sun-angle diffuse.
const vec3 faceNormals[6] = vec3[6](
    vec3( 0, 1, 0),   // top
    vec3( 0,-1, 0),   // bottom
    vec3( 1, 0, 0),   // +X
    vec3(-1, 0, 0),   // -X
    vec3( 0, 0, 1),   // +Z
    vec3( 0, 0,-1)    // -Z
);

void main()
{
    TexCoord  = aUV;
    TexLayer  = int(aLayer);
    FaceLight = aLight;

    vec3 worldPos = vec3(model * vec4(aPos, 1.0));
    FragPos = worldPos;
    
    // Calculate fragment position in light space for shadow mapping
    FragPosLightSpace = lightSpaceMatrix * vec4(worldPos, 1.0);

    // aLight baked: top=1.0, sides=0.8, bottom=0.6 — map back to face index
    int faceIdx = 0;
    if      (aLight > 0.95) faceIdx = 0;
    else if (aLight < 0.65) faceIdx = 1;
    else if (aLight < 0.75) faceIdx = 5;
    else                    faceIdx = 2;
    Normal = faceNormals[faceIdx];

    gl_Position = projection * view * vec4(worldPos, 1.0);
}
