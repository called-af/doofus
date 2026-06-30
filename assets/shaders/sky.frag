#version 330 core

in vec3 viewRay;
out vec4 FragColor;

uniform mat4 invProj;
uniform mat4 invView;

uniform vec3 uTopColor;
uniform vec3 uHorizonColor;
uniform vec3 uBottomColor;
uniform vec3 uSunDir;
uniform vec3 uMoonDir;

void main()
{
    vec4 ray = invProj * vec4(viewRay, 1.0);
    ray = vec4(ray.xyz, 0.0);

    vec3 worldDir = normalize((invView * ray).xyz);

    float t = worldDir.y * 0.5 + 0.5;

    vec3 sky =
        mix(uBottomColor, uHorizonColor, smoothstep(0.0, 0.5, t));

    sky =
        mix(sky, uTopColor, smoothstep(0.5, 1.0, t));

    // Sun disc and glow
    float sunDist = distance(worldDir, normalize(uSunDir));
    float sunDisc = smoothstep(0.05, 0.02, sunDist);
    float sunGlow = exp(-sunDist * sunDist * 50.0) * 0.6;
    
    vec3 sunColor = mix(vec3(1.0, 0.9, 0.7), vec3(1.0, 0.5, 0.0), 0.3);
    sky = mix(sky, sunColor, sunDisc);
    sky += sunColor * sunGlow * 0.5;

    // Moon disc and subtle glow
    float moonDist = distance(worldDir, normalize(uMoonDir));
    float moonDisc = smoothstep(0.04, 0.015, moonDist);
    float moonGlow = exp(-moonDist * moonDist * 30.0) * 0.3;
    
    vec3 moonColor = vec3(0.95, 0.95, 0.98);
    sky = mix(sky, moonColor, moonDisc);
    sky += moonColor * moonGlow * 0.3;

    FragColor = vec4(sky, 1.0);
}