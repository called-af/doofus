#version 460 core

layout(location=0) in vec3 aPos;
layout(location=1) in vec2 aUV;
layout(location=2) in float aLayer;
layout(location=3) in float aLight;

out vec2 TexCoord;
flat out int TexLayer;

uniform mat4 model;
uniform mat4 lightSpaceMatrix;

// Must be identical to block.vert — shadow map depth MUST match
// FragPosLightSpace computed in block.vert. If shadow.vert does not
// apply the same spawn animation, vertex positions in the shadow map differ
// from the positions used by block.vert to compute FragPosLightSpace
// → shadow will not stick to objects.
uniform int   uIsLOD;
uniform float uTime;
uniform float uLodSpawnTime;

const float CHUNK_SPAWN_DURATION = 0.5;
const float CHUNK_RISE_OFFSET    = 8.0;
const float LOD_SPAWN_DURATION   = 1.2;
const float LOD_RISE_OFFSET      = 18.0;

void main()
{
    TexCoord = aUV;
    TexLayer = int(aLayer);

    vec3 pos = aPos;

    // Spawn animation — identical to block.vert
    if (uLodSpawnTime >= 0.0) {
        float elapsed  = uTime - uLodSpawnTime;
        float duration = (uIsLOD == 1) ? LOD_SPAWN_DURATION : CHUNK_SPAWN_DURATION;
        float rise     = (uIsLOD == 1) ? LOD_RISE_OFFSET    : CHUNK_RISE_OFFSET;
        float t        = clamp(elapsed / duration, 0.0, 1.0);
        float ease     = t * t * (3.0 - 2.0 * t);
        pos.y += mix(-rise, 0.0, ease);
    }

    vec3 worldPos = vec3(model * vec4(pos, 1.0));
    gl_Position = lightSpaceMatrix * vec4(worldPos, 1.0);
}
