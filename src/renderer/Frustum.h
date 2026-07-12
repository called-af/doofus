#pragma once

#include <glm/glm.hpp>

class Frustum {
public:
    glm::vec4 planes[6];

    void update(const glm::mat4& projection,
                const glm::mat4& view);

    bool isBoxVisible(
        const glm::vec3& minBounds,
        const glm::vec3& maxBounds
    ) const;
};
