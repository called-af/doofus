#pragma once
#include "../../core/Setting.h"

#include <glm/glm.hpp>

struct RigidbodyComponent {
  glm::vec3 velocity = glm::vec3(0.0f);

  bool grounded = false;

  float gravity = Setting::gravity;
  float jumpForce = Setting::jumpForce;
};