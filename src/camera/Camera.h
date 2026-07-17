#pragma once
#include "../core/Setting.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

enum class CameraMode {
  FirstPerson = 0,
  ThirdPersonBack = 1,
  ThirdPersonFront = 2,
  FreeCamera = 3
};

class Camera {
public:
  glm::vec3 position = {Setting::spawnX,
                        Setting::spawnY + Setting::cameraEyeHeight,
                        Setting::spawnZ};

  glm::vec3 front = glm::vec3(0.0f, 0.0f, -1.0f);

  glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

  float yaw = Setting::defaultYaw;
  float pitch = Setting::defaultPitch;

  CameraMode mode = CameraMode::FirstPerson;

  Camera();

  glm::mat4 getViewMatrix();

  void updateVectors();
};