#include "Camera.h"

#include <glm/gtc/matrix_transform.hpp>

Camera::Camera() { updateVectors(); }

glm::mat4 Camera::getViewMatrix() {
  if (mode == CameraMode::ThirdPersonFront) {
    return glm::lookAt(position, position - front, up);
  }
  return glm::lookAt(position, position + front, up);
}

void Camera::updateVectors() {
  glm::vec3 direction;

  direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));

  direction.y = sin(glm::radians(pitch));

  direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));

  front = glm::normalize(direction);
}

void Camera::updateFov(float dt) {
  currentFov += (targetFov - currentFov) * std::min(1.0f, dt * 18.0f);
}