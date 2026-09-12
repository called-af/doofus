#version 460 core

in vec2 TexCoord;
in vec3 FragPos;
in vec4 FragPosLightSpace;

out vec4 FragColor;

uniform sampler2D tex;
uniform sampler2D shadowMap;

uniform mat4 uCascadeLightSpace[4];
uniform mat4 view;

uniform vec3 cameraPos;
uniform vec3 fogColor;
uniform float fogStart;
uniform float fogEnd;

uniform vec3 uLightDir;
uniform vec3 uLightColor;
uniform vec3 uAmbientColor;
uniform float uShadowDistance;
uniform int uShadowsEnabled;

float calculateShadow(vec3 worldPos, vec3 normal, float ndotl)
{
    vec4 viewPos = view * vec4(worldPos, 1.0);
    int cascadeIdx = int(max(sign(viewPos.x), 0.0) + 2.0 * max(sign(viewPos.y), 0.0));

    float normalOffset = max(0.04 * (1.0 - ndotl), 0.01);
    vec3 biasedPos = worldPos + normal * normalOffset;
    vec4 lightSpacePos = uCascadeLightSpace[cascadeIdx] * vec4(biasedPos, 1.0);

    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    projCoords = projCoords * 0.5 + 0.5;

    if (projCoords.z > 1.0 || projCoords.z < 0.0 ||
        projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0)
        return 0.0;

    float atlasX = (projCoords.x + float(cascadeIdx)) * 0.25;
    vec2 shadowCoord = vec2(atlasX, projCoords.y);

    float currentDepth = projCoords.z - 0.00004;
    vec2 texelSize = 1.0 / vec2(textureSize(shadowMap, 0));
    float shadow = 0.0;
    vec2 offset = texelSize * 0.5;

    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(shadowMap, shadowCoord + vec2(x, y) * offset).r;
            shadow += (currentDepth > pcfDepth) ? 1.0 : 0.0;
        }
    }
    return shadow / 9.0;
}

float calcFogFactor(float dist, float fStart, float fEnd)
{
    if (fEnd <= fStart || dist <= fStart) return 1.0;
    float t = clamp((dist - fStart) / (fEnd - fStart), 0.0, 1.0);
    return 1.0 - t * t;
}

void main()
{
    vec4 texColor = texture(tex, TexCoord);
    if (texColor.a < 0.05) discard;

    // Compute real geometric face normal from screen-space derivatives
    vec3 geomNormal = normalize(cross(dFdx(FragPos), dFdy(FragPos)));
    if (!gl_FrontFacing) geomNormal = -geomNormal;

    float ndotl = max(dot(geomNormal, uLightDir), 0.0);

    const float cameraDistance = length(FragPos - cameraPos);
    float shadow = 0.0;
    if (uShadowsEnabled == 1 && cameraDistance <= uShadowDistance) {
        shadow = calculateShadow(FragPos, geomNormal, ndotl);
        float fadeStart = uShadowDistance * 0.85;
        if (cameraDistance > fadeStart) {
            float fade = 1.0 - clamp((cameraDistance - fadeStart) / (uShadowDistance - fadeStart), 0.0, 1.0);
            shadow *= fade;
        }
    }

    vec3 direct = uLightColor * ndotl * (1.0 - shadow * 0.85);
    vec3 light = uAmbientColor + direct;
    light = max(light, vec3(0.20));

    vec3 lit = texColor.rgb * light;

    float ff = calcFogFactor(cameraDistance, fogStart, fogEnd);
    vec3 color = mix(fogColor, lit, ff);

    FragColor = vec4(color, texColor.a);
}
