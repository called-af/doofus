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
out float vSpawnT;   // 0..1 spawn animation progress

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

uniform int   uIsLOD;
uniform vec3  uLightDir;    // needed in vertex shader for NdotL

uniform float uTime;
uniform float uLodSpawnTime;

const float CHUNK_SPAWN_DURATION = 0.5;
const float CHUNK_RISE_OFFSET    = 8.0;
const float LOD_SPAWN_DURATION   = 1.2;
const float LOD_RISE_OFFSET      = 18.0;

const vec3 faceNormals[6] = vec3[6](
    vec3( 0, 1, 0),
    vec3( 0,-1, 0),
    vec3( 1, 0, 0),
    vec3(-1, 0, 0),
    vec3( 0, 0, 1),
    vec3( 0, 0,-1)
);

void main()
{
    TexCoord  = aUV;
    TexLayer  = int(aLayer);
    FaceLight = aLight;

    // ── Normal derived from baked light tier ──────────────────────────────────────
    int faceIdx = 0;
    if      (aLight > 0.95) faceIdx = 0;   // top    → (0, 1, 0)
    else if (aLight < 0.60) faceIdx = 1;   // bottom → (0,-1, 0)
    else if (aLight < 0.75) faceIdx = 5;   // Z-axis → (0, 0,-1)  (conservative)
    else                    faceIdx = 2;   // X-axis → (1, 0, 0)  (conservative)
    Normal = faceNormals[faceIdx];

    // ── Spawn animation ──────────────────────────────────────────────────────
    vec3 pos = aPos;
    vSpawnT  = 1.0;

    if (uIsLOD == 0 && uLodSpawnTime >= 0.0) {
        float elapsed  = uTime - uLodSpawnTime;
        float duration = CHUNK_SPAWN_DURATION;
        float rise     = CHUNK_RISE_OFFSET;
        float t        = clamp(elapsed / duration, 0.0, 1.0);
        float ease     = t * t * (3.0 - 2.0 * t);
        pos.y += mix(-rise, 0.0, ease);
        vSpawnT = ease;
    }

    vec3 worldPos = vec3(model * vec4(pos, 1.0));
    FragPos       = worldPos;

    gl_Position = projection * view * vec4(worldPos, 1.0);
}
