vec3 applyLighting(vec3 baseColor, float vertexLight, vec3 skyTopColor) {
    vec3 ambientLight = max(skyTopColor, vec3(0.15)); 
    
    return baseColor * ambientLight * vertexLight;
}