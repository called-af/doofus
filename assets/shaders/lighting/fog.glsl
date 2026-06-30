vec3 applyFog(vec3 origColor, vec3 fogColor, float distance, float fogStart, float fogEnd) {
    float fogFactor = (fogEnd - distance) / (fogEnd - fogStart);
    fogFactor = clamp(fogFactor, 0.0, 1.0);
    
    return mix(fogColor, origColor, fogFactor);
}