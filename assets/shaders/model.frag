#version 460 core

in vec2 TexCoord;
in vec3 FragPos;

out vec4 FragColor;

uniform sampler2D tex;

uniform vec3  cameraPos;
uniform vec3  fogColor;
uniform float fogStart;
uniform float fogEnd;

uniform vec3  uLightDir;
uniform vec3  uLightColor;
uniform vec3  uAmbientColor;

// ─── Point Lights ─────────────────────────────────────────────────────────
struct PointLight {
    vec3  position;
    vec3  color;
    float radius;
    float intensity;
};
uniform int        uNumPointLights;
uniform PointLight uPointLights[8];

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

    vec3 geomNormal = normalize(cross(dFdx(FragPos), dFdy(FragPos)));
    if (!gl_FrontFacing) geomNormal = -geomNormal;

    float ndotl = max(dot(geomNormal, uLightDir), 0.0);

    vec3 direct = uLightColor * ndotl;
    vec3 light  = uAmbientColor + direct;

    // ─── Point Lights ─────────────────────────────────────────────────────
    for (int i = 0; i < uNumPointLights; ++i) {
        float d = length(uPointLights[i].position - FragPos);
        if (d < uPointLights[i].radius) {
            float att = 1.0 / (1.0 + 0.09 * d + 0.032 * d * d);
            att *= (1.0 - smoothstep(uPointLights[i].radius * 0.75, uPointLights[i].radius, d));
            vec3  toLight = normalize(uPointLights[i].position - FragPos);
            float nDotPL  = max(dot(geomNormal, toLight), 0.0);
            light += uPointLights[i].color * uPointLights[i].intensity * att * (0.6 + 0.4 * nDotPL);
        }
    }

    light = max(light, vec3(0.20));
    vec3 lit   = texColor.rgb * light;
    float ff   = calcFogFactor(length(FragPos - cameraPos), fogStart, fogEnd);
    vec3 color = mix(fogColor, lit, ff);

    FragColor = vec4(color, texColor.a);
}
