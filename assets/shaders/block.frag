#version 460 core

in vec2  TexCoord;
flat in int   TexLayer;
in vec3  FragPos;
in float FaceLight;
in vec3  Normal;
in vec4  FragPosLightSpace;

out vec4 FragColor;

uniform sampler2DArray atlas;
uniform sampler2D shadowMap;

uniform vec3  cameraPos;
uniform vec3  fogColor;
uniform float fogStart;
uniform float fogEnd;

uniform vec3  uLightDir;
uniform vec3  uLightColor;
uniform vec3  uAmbientColor;
uniform float uShadowDistance;
uniform int   uShadowsEnabled;

// Selective vibrance boost: raises saturation of less-saturated colors without blowing out already saturated ones
vec3 boostVibrance(vec3 color, float amount) {
    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
    float maxChan = max(color.r, max(color.g, color.b));
    float sat = maxChan - luminance;
    return mix(vec3(luminance), color, 1.0 + amount * (1.0 - sat));
}

float calculateShadow(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir)
{
    // Next-level optimization: Orthographic projection has w = 1.0, so we skip the expensive float division!
    vec3 projCoords = fragPosLightSpace.xyz * 0.5 + 0.5;
    
    // Out of shadow map bounds
    if(projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || 
       projCoords.y < 0.0 || projCoords.y > 1.0) {
        return 0.0; 
    }
    
    // Precise constant bias relative to the depth range (400.0) to prevent both shadow acne and peter-panning (detached shadows)
    float bias = max(0.0006 * (1.0 - dot(normal, lightDir)), 0.00015);
    
    // Beautiful retro pixelated hard shadow (single texture fetch, extremely fast!)
    float shadowDepth = texture(shadowMap, projCoords.xy).r;
    float shadow = (projCoords.z - bias > shadowDepth) ? 1.0 : 0.0;
    
    return shadow;
}

void main()
{
    vec4 texColor = texture(atlas, vec3(fract(TexCoord), TexLayer));

    // Directional diffuse from active light source
    float NdotL = max(dot(Normal, uLightDir), 0.0);
    
    // Skip shadow calculations completely at night for next-level optimization!
    float shadow = 0.0;
    if (uShadowsEnabled == 1) {
        shadow = calculateShadow(FragPosLightSpace, Normal, uLightDir);
        
        // Fade shadow near the shadow distance boundary
        float distToPlayer = length(FragPos - cameraPos);
        float shadowFadeStart = uShadowDistance * 0.75;
        float shadowFade = clamp((uShadowDistance - distToPlayer) / (uShadowDistance - shadowFadeStart), 0.0, 1.0);
        shadow *= shadowFade;
    }
    
    // Direct light contribution (soften shadows slightly so they aren't pitch black)
    float shadowStrength = 0.82;
    vec3 directLight = uLightColor * NdotL * (1.0 - shadow * shadowStrength);
    
    // Total light (ambient + direct)
    vec3 light = uAmbientColor + directLight;
    
    // Apply baked face light multiplier (flat AO + face directional shading)
    light *= FaceLight;
    
    // Ensure minimum brightness for visibility in dark/caves
    light = max(light, vec3(0.06));
    
    // Final lit color
    vec3 lit = texColor.rgb * light;

    // --- High-Fidelity Color Grading ---
    // 1. Boost vibrance selectively by 65% for punchy voxel tones
    lit = boostVibrance(lit, 0.65);
    // 2. Extra 25% overall saturation boost to prevent washed-up textures
    float luminance = dot(lit, vec3(0.2126, 0.7152, 0.0722));
    lit = mix(vec3(luminance), lit, 1.25);
    
    // Clean, natural contrast/gamma lift (1.1) to keep shadows visible and colors rich
    lit = pow(lit, vec3(1.0 / 1.1));

    // Fog calculation
    float dist = length(FragPos - cameraPos);
    float fog  = clamp((fogEnd - dist) / (fogEnd - fogStart), 0.0, 1.0);
    vec3 fogged = mix(fogColor, lit, fog);

    FragColor = vec4(fogged, texColor.a);
}
