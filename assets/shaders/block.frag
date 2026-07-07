#version 460 core

in vec2  TexCoord;
flat in int   TexLayer;
in vec3  FragPos;
in float FaceLight;
in vec3  Normal;
in vec4  FragPosLightSpace;
in float vNdotL;    // pre-computed NdotL from vertex shader
in float vSpawnT;   // 0..1 spawn animation progress

out vec4 FragColor;

uniform sampler2DArray atlas;
uniform sampler2D      shadowMap;

uniform vec3  cameraPos;
uniform vec3  fogColor;
uniform float fogStart;
uniform float fogEnd;

uniform vec3  uLightDir;
uniform vec3  uLightColor;
uniform vec3  uAmbientColor;
uniform float uShadowDistance;
uniform int   uShadowsEnabled;
uniform int   uIsLOD;

// ─────────────────────────────────────────────────────────────────────────────
//  PCF Shadow — smooth, follows object shape, elongates based on sun position
//
//  PCF (Percentage Closer Filtering): samples depth in a 3×3 neighbourhood,
//  averages the results. Produces shadow with soft edges that follow
//  geometry shape, not pixelated per texel.
//
//  Bias: linear between 0.0003 (top face, NdotL=1) and 0.002 (side face, NdotL=0).
//  Small but sufficient — shadow sticks tightly to the top surface of blocks.
// ─────────────────────────────────────────────────────────────────────────────
float calculateShadow(vec4 fragPosLightSpace, float ndotl)
{
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    // Outside shadow frustum → no shadow
    if (projCoords.z > 1.0 ||
        projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0)
        return 0.0;

    // Linear bias: very small for top face, slightly larger for side faces
    float bias = mix(0.0003, 0.002, 1.0 - clamp(ndotl, 0.0, 1.0));

    // PCF 3×3: sample 9 points around projCoords, average the results
    // textureSize(shadowMap, 0) = shadow map resolution → texelSize = 1 texel
    vec2 texelSize = 1.0 / vec2(textureSize(shadowMap, 0));
    float shadow = 0.0;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += (projCoords.z - bias > pcfDepth) ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;

    return shadow;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Phong Specular
//
//  reflect(-L, N) → R, kemudian pow(max(dot(V,R), 0), shininess)
//  For voxel terrain: low shininess (16) and small intensity (0.12)
//  to avoid a too "plastic" appearance.
// ─────────────────────────────────────────────────────────────────────────────
vec3 calcSpecular(vec3 normal, vec3 lightDir, vec3 viewDir, vec3 lightColor)
{
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec      = pow(max(dot(viewDir, reflectDir), 0.0), 16.0);
    return lightColor * spec * 0.12;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Exponential-squared fog
// ─────────────────────────────────────────────────────────────────────────────
float calcFogFactor(float dist, float fStart, float fEnd)
{
    if (dist <= fStart) return 1.0;
    float t = clamp((dist - fStart) / (fEnd - fStart), 0.0, 1.0);
    return 1.0 - t * t;
}

void main()
{
    vec4 texColor = texture(atlas, vec3(fract(TexCoord), TexLayer));
    if (texColor.a < 0.01) discard;

    // ─── Shadow ───────────────────────────────────────────────────────────
    float shadow = 0.0;
    if (uIsLOD == 0 && uShadowsEnabled == 1) {
        shadow = calculateShadow(FragPosLightSpace, vNdotL);
    }

    // ─── Diffuse (Lambert) ────────────────────────────────────────────────
    // vNdotL already computed in vertex shader
    vec3 direct = uLightColor * vNdotL * (1.0 - shadow * 0.85);

    // ─── Phong Specular ─────────────────────────────────────────────────
    // Only for full-detail chunks (not LOD) and not for surfaces
    // facing downward (faceIdx bottom → aLight < 0.60)
    vec3 specular = vec3(0.0);
    if (uIsLOD == 0 && vNdotL > 0.0 && uShadowsEnabled == 1) {
        vec3 viewDir = normalize(cameraPos - FragPos);
        specular = calcSpecular(Normal, uLightDir, viewDir, uLightColor);
        // Reduce specular in shadowed areas
        specular *= (1.0 - shadow);
    }

    // ─── Combined lighting ────────────────────────────────────────────────
    vec3 light = uAmbientColor + direct + specular;
    light     *= FaceLight;

    if (uIsLOD == 1) {
        light = mix(light, vec3(dot(light, vec3(0.299, 0.587, 0.114))), 0.15);
        light = max(light, vec3(0.12));
    } else {
        light = max(light, vec3(0.07));
    }

    vec3 lit   = texColor.rgb * light;

    // ─── Fog ──────────────────────────────────────────────────────────────
    float dist              = length(FragPos - cameraPos);
    float effectiveFogStart = (uIsLOD == 1) ? fogStart * 0.75 : fogStart;
    float ff                = calcFogFactor(dist, effectiveFogStart, fogEnd);
    vec3  color             = mix(fogColor, lit, ff);

    // ─── Spawn animation: fade-in ────────────────────────────────────────────
    float alpha = texColor.a;
    if (vSpawnT < 1.0) {
        float fadeStart = (uIsLOD == 1) ? 0.3 : 0.0;
        float fadeT     = clamp((vSpawnT - fadeStart) / (1.0 - fadeStart), 0.0, 1.0);
        fadeT           = fadeT * fadeT * (3.0 - 2.0 * fadeT);
        color           = mix(fogColor, color, fadeT);
        alpha          *= fadeT;
    }

    FragColor = vec4(color, alpha);
}
