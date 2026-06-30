#ifndef MATH_GLSL
#define MATH_GLSL

// Fungsi pembantu jika dibutuhkan di masa depan
float linsstep(float low, float high, float v) {
    return clamp((v - low) / (high - low), 0.0, 1.0);
}

#endif