#pragma once
#include <glm/glm.hpp>

struct RigidbodyComponent {
    glm::vec3 velocity = glm::vec3(0.0f);

    bool grounded = false;

    float gravity = -20.0f;
    float jumpForce = 8.0f;
};