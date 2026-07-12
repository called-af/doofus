#include "Frustum.h"

void Frustum::update(
    const glm::mat4& projection,
    const glm::mat4& view
) {
    glm::mat4 m = projection * view;

    planes[0] = glm::vec4(
        m[0][3] + m[0][0],
        m[1][3] + m[1][0],
        m[2][3] + m[2][0],
        m[3][3] + m[3][0]
    );

    planes[1] = glm::vec4(
        m[0][3] - m[0][0],
        m[1][3] - m[1][0],
        m[2][3] - m[2][0],
        m[3][3] - m[3][0]
    );

    planes[2] = glm::vec4(
        m[0][3] + m[0][1],
        m[1][3] + m[1][1],
        m[2][3] + m[2][1],
        m[3][3] + m[3][1]
    );

    planes[3] = glm::vec4(
        m[0][3] - m[0][1],
        m[1][3] - m[1][1],
        m[2][3] - m[2][1],
        m[3][3] - m[3][1]
    );

    planes[4] = glm::vec4(
        m[0][3] + m[0][2],
        m[1][3] + m[1][2],
        m[2][3] + m[2][2],
        m[3][3] + m[3][2]
    );

    planes[5] = glm::vec4(
        m[0][3] - m[0][2],
        m[1][3] - m[1][2],
        m[2][3] - m[2][2],
        m[3][3] - m[3][2]
    );

    for (int i = 0; i < 6; i++) {

        float len = glm::length(
            glm::vec3(planes[i])
        );

        planes[i] /= len;
    }
}

bool Frustum::isBoxVisible(
    const glm::vec3& minBounds,
    const glm::vec3& maxBounds
) const {

    for (int i = 0; i < 6; i++) {

        glm::vec3 p;

        p.x = planes[i].x > 0 ? maxBounds.x : minBounds.x;
        p.y = planes[i].y > 0 ? maxBounds.y : minBounds.y;
        p.z = planes[i].z > 0 ? maxBounds.z : minBounds.z;

        if (
            planes[i].x * p.x +
            planes[i].y * p.y +
            planes[i].z * p.z +
            planes[i].w < 0
        ) {
            return false;
        }
    }

    return true;
}
