#pragma once
#include <glm/glm.hpp>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
//  PointLight — a local omni-directional light source with attenuation.
//
//  Usage (from anywhere in the codebase):
//
//    // Register a torch at world position (x, y, z):
//    LightRegistry::add({ glm::vec3(x, y, z), glm::vec3(1.0f, 0.6f, 0.2f), 12.0f, 1.8f });
//
//    // Clear all lights for the frame (call once at frame start):
//    LightRegistry::clear();
//
//  The Scene picks up LightRegistry::getAll() every frame and uploads it
//  to the block and model shaders automatically.
//
//  Shader side:
//    struct PointLight { vec3 pos; vec3 color; float radius; float intensity; };
//    The attenuation formula used in the fragment shader is:
//      att = 1.0 / (1.0 + 0.09*d + 0.032*d*d)   (clamped to [0, radius])
// ─────────────────────────────────────────────────────────────────────────────

struct PointLight {
    glm::vec3 position  = glm::vec3(0.0f);
    glm::vec3 color     = glm::vec3(1.0f);   // RGB, typically warm (1, 0.6, 0.2) for torch
    float     radius    = 10.0f;             // world-unit cutoff distance
    float     intensity = 1.0f;              // multiplier on top of attenuation
};

class LightRegistry {
public:
    static constexpr int MAX_LIGHTS = 8;

    // Register a light for this frame. Silently ignored if MAX_LIGHTS is reached.
    static void add(const PointLight& light) {
        if ((int)lights_.size() < MAX_LIGHTS)
            lights_.push_back(light);
    }

    // Clear all registered lights (call at the start of each frame).
    static void clear() {
        lights_.clear();
    }

    static const std::vector<PointLight>& getAll() {
        return lights_;
    }

    static int count() {
        return (int)lights_.size();
    }

private:
    static inline std::vector<PointLight> lights_;
};
