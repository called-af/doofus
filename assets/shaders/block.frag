#version 460 core

layout(early_fragment_tests) in;

in vec2  TexCoord;
flat in int   TexLayer;
in vec3  FragPos;
in float FaceLight;
in vec3  Normal;
in float vSpawnT;   // 0..1 spawn animation progress

out vec4 FragColor;

uniform mat4 view;
uniform sampler2DArray atlas;

uniform vec3  cameraPos;
uniform vec3  fogColor;
uniform float fogStart;
uniform float fogEnd;

uniform vec3  uLightDir;
uniform vec3  uLightColor;
uniform vec3  uAmbientColor;
uniform int   uIsLOD;
uniform float uTime;

// ─────────────────────────────────────────────────────────────────────────────
//  Point Lights — up to 8 omni lights with realistic attenuation
// ─────────────────────────────────────────────────────────────────────────────
struct PointLight {
    vec3  position;
    vec3  color;
    float radius;
    float intensity;
};
uniform int        uNumPointLights;
uniform PointLight uPointLights[8];

// ─────────────────────────────────────────────────────────────────────────────
//  Phong Specular
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

    // Exact geometric face normal computed from derivatives (perfect voxel + slope normals)
    vec3 geomNormal = normalize(cross(dFdx(FragPos), dFdy(FragPos)));
    if (!gl_FrontFacing) geomNormal = -geomNormal;

    float ndotl = max(dot(geomNormal, uLightDir), 0.0);

    // ─── Diffuse (Lambert) ────────────────────────────────────────────────
    vec3 direct = uLightColor * ndotl;

    // ─── Phong Specular ─────────────────────────────────────────────────
    vec3 specular = vec3(0.0);
    if (uIsLOD == 0 && ndotl > 0.0) {
        vec3 viewDir = normalize(cameraPos - FragPos);
        specular = calcSpecular(geomNormal, uLightDir, viewDir, uLightColor);
    }

    // ─── Combined lighting ────────────────────────────────────────────────
    vec3 light = uAmbientColor + direct + specular;
    light     *= FaceLight;

    // ─── Point Lights (local omni sources — torches, lava glow, etc.) ────
    if (uIsLOD == 0) {
        for (int i = 0; i < uNumPointLights; ++i) {
            float dist = length(uPointLights[i].position - FragPos);
            if (dist < uPointLights[i].radius) {
                // Quadratic attenuation: full at dist=0, zero at dist=radius
                float att = 1.0 / (1.0 + 0.09 * dist + 0.032 * dist * dist);
                // Smooth falloff at edge of radius
                att *= (1.0 - smoothstep(uPointLights[i].radius * 0.75, uPointLights[i].radius, dist));
                vec3 toLight = normalize(uPointLights[i].position - FragPos);
                float nDotPL = max(dot(geomNormal, toLight), 0.0);
                light += uPointLights[i].color * uPointLights[i].intensity * att * (0.6 + 0.4 * nDotPL);
            }
        }
    }

    if (uIsLOD == 1) {
        light = mix(light, vec3(dot(light, vec3(0.299, 0.587, 0.114))), 0.15);
        light = max(light, vec3(0.12));
    } else {
        light = max(light, vec3(0.07));
    }

    vec3 lit   = texColor.rgb * light;

    // ─── Emissive Glow & Material Shading Enhancements ───────────────────
    if (TexLayer == 6) {
        // Layer 6: Molten Lava (Incandescent fiery magma flow & pulsation)
        float wave1 = sin(FragPos.x * 0.9 + FragPos.z * 0.9 + uTime * 2.2);
        float wave2 = cos(FragPos.x * 0.6 - FragPos.z * 0.7 + uTime * 1.6);
        float heatPulse = 0.88 + 0.12 * (wave1 * 0.5 + wave2 * 0.5);

        // Vibrant glowing magma core color (radiates intensely even in dark caves/shadows)
        vec3 lavaCore = texColor.rgb * vec3(1.35, 1.15, 0.85) * heatPulse;
        lit = mix(lit * 0.25 + lavaCore * 0.80, lavaCore, 0.85);
    }
    else if (TexLayer == 9) {
        // Layer 9: Cinder / Burning Embers
        float emberPulse = 0.5 + 0.5 * sin(FragPos.x * 3.2 + FragPos.z * 3.2 + uTime * 2.8);
        vec3 emberGlow = vec3(0.55, 0.18, 0.03) * (emberPulse * emberPulse);
        lit += emberGlow * (1.0 - ndotl * 0.4);
    }
    else if (TexLayer == 11) {
        // Layer 11: Heaven Crystal
        float shimmer = 0.5 + 0.5 * sin(FragPos.y * 2.2 + FragPos.x + uTime * 2.5);
        lit += vec3(0.18, 0.38, 0.55) * (shimmer * 0.40);
    }

    // ─── Fog ──────────────────────────────────────────────────────────────
    float dist              = length(FragPos - cameraPos);
    float effectiveFogStart = (uIsLOD == 1) ? fogStart * 0.75 : fogStart;
    float ff                = calcFogFactor(dist, effectiveFogStart, fogEnd);
    
    vec3 effectiveFogColor  = fogColor;
    if (TexLayer == 6) {
        // Lava's bright fiery radiance punches slightly through volcanic fog
        effectiveFogColor = mix(fogColor, vec3(0.65, 0.25, 0.08), 0.22);
        ff = clamp(ff * 1.12, 0.0, 1.0);
    }

    vec3 color = mix(effectiveFogColor, lit, ff);

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
