#version 460 core

layout(early_fragment_tests) in;

in vec2  TexCoord;
flat in int   TexLayer;
in vec3  FragPos;
in float FaceLight;
in vec3  Normal;
in vec4  FragPosLightSpace;
in float vNdotL;    // pre-computed NdotL from vertex shader
in float vSpawnT;   // 0..1 spawn animation progress

out vec4 FragColor;

uniform mat4 uCascadeLightSpace[4];
uniform mat4 view;
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
uniform float uTime;

// ─────────────────────────────────────────────────────────────────────────────
//  Cascaded Shadow Mapping (Horizontal & Vertical Splits) with Normal-Offset Bias
// ─────────────────────────────────────────────────────────────────────────────
float calculateShadow(vec3 worldPos, vec3 normal, float ndotl)
{
    if (ndotl <= 0.0001) return 1.0; // Self-shadowing on faces angled away from sun

    // ── Horizontal and Vertical Split Cascade Index ───────────────────────────
    // Camera view space: +X is right, +Y is up, -Z is view direction
    // Quadrant 0 = Left-Bottom  (viewPos.x < 0,  viewPos.y < 0)
    // Quadrant 1 = Right-Bottom (viewPos.x >= 0, viewPos.y < 0)
    // Quadrant 2 = Left-Top     (viewPos.x < 0,  viewPos.y >= 0)
    // Quadrant 3 = Right-Top    (viewPos.x >= 0, viewPos.y >= 0)
    vec4 viewPos = view * vec4(worldPos, 1.0);
    int cascadeIdx = int(max(sign(viewPos.x), 0.0) + 2.0 * max(sign(viewPos.y), 0.0));

    // Shift shadow test position outward along surface normal (avoids acne while keeping box corners sharp)
    float normalOffset = max(0.05 * (1.0 - ndotl), 0.012);
    vec3 biasedPos = worldPos + normal * normalOffset;
    vec4 lightSpacePos = uCascadeLightSpace[cascadeIdx] * vec4(biasedPos, 1.0);

    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    projCoords = projCoords * 0.5 + 0.5;

    // Outside shadow frustum → no shadow
    if (projCoords.z > 1.0 || projCoords.z < 0.0 ||
        projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0)
        return 0.0;

    // Map into the 4x1 horizontal cascade atlas texture
    float atlasX = (projCoords.x + float(cascadeIdx)) * 0.25;
    vec2 shadowCoord = vec2(atlasX, projCoords.y);

    float currentDepth = projCoords.z - 0.00003;
    vec2 texelSize = 1.0 / vec2(textureSize(shadowMap, 0));

    // 4-tap sub-texel filter inside the cascade tile
    float shadow = 0.0;
    vec2 offset = texelSize * 0.45;
    
    shadow += (currentDepth > texture(shadowMap, shadowCoord + vec2(-offset.x, -offset.y)).r) ? 1.0 : 0.0;
    shadow += (currentDepth > texture(shadowMap, shadowCoord + vec2( offset.x, -offset.y)).r) ? 1.0 : 0.0;
    shadow += (currentDepth > texture(shadowMap, shadowCoord + vec2(-offset.x,  offset.y)).r) ? 1.0 : 0.0;
    shadow += (currentDepth > texture(shadowMap, shadowCoord + vec2( offset.x,  offset.y)).r) ? 1.0 : 0.0;
    shadow *= 0.25;

    return shadow;
}

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

    // ─── Shadow ───────────────────────────────────────────────────────────
    const float cameraDistance = length(FragPos - cameraPos);
    float shadow = 0.0;
    if (uIsLOD == 0 && uShadowsEnabled == 1 && cameraDistance <= uShadowDistance) {
        shadow = calculateShadow(FragPos, geomNormal, ndotl);
        // Smooth fade out near the max shadow distance edge
        float fadeStart = uShadowDistance * 0.85;
        if (cameraDistance > fadeStart) {
            float fade = 1.0 - clamp((cameraDistance - fadeStart) / (uShadowDistance - fadeStart), 0.0, 1.0);
            shadow *= fade;
        }
    }

    // ─── Diffuse (Lambert) ────────────────────────────────────────────────
    vec3 direct = uLightColor * ndotl * (1.0 - shadow * 0.90);

    // ─── Phong Specular ─────────────────────────────────────────────────
    vec3 specular = vec3(0.0);
    if (uIsLOD == 0 && ndotl > 0.0 && uShadowsEnabled == 1
        && cameraDistance <= uShadowDistance) {
        vec3 viewDir = normalize(cameraPos - FragPos);
        specular = calcSpecular(geomNormal, uLightDir, viewDir, uLightColor);
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
    float dist              = cameraDistance;
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
