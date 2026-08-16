#include "PlayerController.h"

#include "../core/Setting.h"
#include "../core/Time.h"
#include "../platform/input/Input.h"
#include "../world/World.h"
#include "../world/block/BlockType.h"

#include <SDL3/SDL.h>

#include <glm/glm.hpp>

#include <cmath>

void PlayerController::update(Camera &camera, TransformComponent &transform,
                              RigidbodyComponent &rigidbody, bool &cursorLocked,
                              SDL_Window *window, Time &time, World &world, float dt) {

  /*
      CURSOR TOGGLE
  */

  if (Input::escapePressed) {
    cursorLocked = !cursorLocked;

    SDL_SetWindowRelativeMouseMode(window, cursorLocked);
  }
   if (Input::bPressed)
    debugVisible = !debugVisible;

  if (Input::bracketLeftPressed) {
    Setting::shadowDistance = std::max(1, Setting::shadowDistance - 1);
  }
  if (Input::bracketRightPressed) {
    Setting::shadowDistance = std::min(Setting::renderDistance, Setting::shadowDistance + 1);
  }
  if (Input::oPressed) {
    Setting::enableShadows = !Setting::enableShadows;
  }
  if (Input::kPressed) {
    Setting::maxLODLevel = std::max(0, Setting::maxLODLevel - 1);
  }
  if (Input::lPressed) {
    Setting::maxLODLevel = std::min(5, Setting::maxLODLevel + 1);
  }

  /*
      ZOOM IN / OUT (Hold C key)
  */

  if (Input::c) {
    camera.targetFov = Setting::fov / 3.0f; // 3x Zoom (30 deg FOV)
  } else {
    camera.targetFov = Setting::fov;
  }
  camera.updateFov(dt);

  /*
      CAMERA MODE TOGGLE (using V key)
  */

  if (Input::vPressed) {
    int nextMode = (static_cast<int>(camera.mode) + 1) % 4;
    camera.mode = static_cast<CameraMode>(nextMode);
  }

  /*
      CAMERA
  */

  if (cursorLocked) {
    float zoomFactor = camera.currentFov / Setting::fov;
    camera.yaw += Input::mouseX * Setting::mouseSensitivity * zoomFactor;

    camera.pitch -= Input::mouseY * Setting::mouseSensitivity * zoomFactor;

    camera.pitch = glm::clamp(camera.pitch, -89.0f, 89.0f);

    camera.updateVectors();
  }

  /*
      MOVEMENT
  */

  if (camera.mode == CameraMode::FreeCamera) {
    // Stop player horizontal movement
    rigidbody.velocity.x = 0.0f;
    rigidbody.velocity.z = 0.0f;

    // Free camera controls
    float speed = 15.0f;
    if (Input::lshift) {
      speed *= 3.0f; // Boost speed
    }

    glm::vec3 right = glm::normalize(glm::cross(camera.front, camera.up));
    glm::vec3 moveDir(0.0f);

    if (Input::w)  moveDir += camera.front;
    if (Input::s)  moveDir -= camera.front;
    if (Input::a)  moveDir -= right;
    if (Input::d)  moveDir += right;

    if (Input::space) moveDir += glm::vec3(0.0f, 1.0f, 0.0f);
    if (Input::lctrl) moveDir -= glm::vec3(0.0f, 1.0f, 0.0f);

    if (glm::length(moveDir) > 0.0f) {
      camera.position += glm::normalize(moveDir) * (speed * dt);
    }
  } else {
    glm::vec3 right = glm::normalize(glm::cross(camera.front, camera.up));

    glm::vec3 flatFront =
        glm::normalize(glm::vec3(camera.front.x, 0.0f, camera.front.z));

    glm::vec3 moveDir(0.0f);

    if (Input::w)
      moveDir += flatFront;

    if (Input::s)
      moveDir -= flatFront;

    if (Input::a)
      moveDir -= right;

    if (Input::d)
      moveDir += right;

    if (glm::length(moveDir) > 0.0f) {
      moveDir = glm::normalize(moveDir);

      rigidbody.velocity.x = moveDir.x * Setting::moveSpeed;

      rigidbody.velocity.z = moveDir.z * Setting::moveSpeed;
    } else {
      rigidbody.velocity.x = 0.0f;
      rigidbody.velocity.z = 0.0f;
    }

    /*
        JUMP
    */

    if (Input::space && rigidbody.grounded) {
      rigidbody.velocity.y = rigidbody.jumpForce;
    }

    /*
        CAMERA FOLLOW
    */

    glm::vec3 headPos = transform.position + glm::vec3(0.0f, Setting::cameraEyeHeight, 0.0f);

    if (camera.mode == CameraMode::FirstPerson) {
      camera.position = headPos;
    } else if (camera.mode == CameraMode::ThirdPersonBack) {
      float maxDistance = 4.0f;
      float distance = maxDistance;
      glm::vec3 checkDir = -camera.front;
      float step = 0.1f;
      for (float t = 0.0f; t <= maxDistance; t += step) {
        glm::vec3 p = headPos + checkDir * t;
        if (world.isSolid((int)std::floor(p.x), (int)std::floor(p.y), (int)std::floor(p.z))) {
          distance = std::max(0.5f, t - 0.2f);
          break;
        }
      }
      camera.position = headPos - camera.front * distance;
    } else if (camera.mode == CameraMode::ThirdPersonFront) {
      float maxDistance = 4.0f;
      float distance = maxDistance;
      glm::vec3 checkDir = camera.front;
      float step = 0.1f;
      for (float t = 0.0f; t <= maxDistance; t += step) {
        glm::vec3 p = headPos + checkDir * t;
        if (world.isSolid((int)std::floor(p.x), (int)std::floor(p.y), (int)std::floor(p.z))) {
          distance = std::max(0.5f, t - 0.2f);
          break;
        }
      }
      camera.position = headPos + camera.front * distance;
    }
  }

  /*
      BLOCK INTERACTION
  */

  if (camera.mode == CameraMode::FirstPerson || camera.mode == CameraMode::ThirdPersonBack) {
    if (Input::left_click) {
      if (time.realTicks - lastBreakTick >= breakCooldown) {
        raycast(camera, world, false);

        lastBreakTick = time.realTicks;
      }
    }

    if (Input::right_click) {
      if (time.realTicks - lastPlaceTick >= placeCooldown) {
        raycast(camera, world, true);

        lastPlaceTick = time.realTicks;
      }
    }
  }
}

void PlayerController::raycast(Camera &camera, World &world, bool place) {
  glm::vec3 pos = camera.position;

  glm::vec3 dir = camera.front;

  glm::vec3 prev = pos;

  float step = 0.1f;

  for (float t = 0; t < reach; t += step) {
    glm::vec3 current = pos + dir * t;

    int x = (int)std::floor(current.x);

    int y = (int)std::floor(current.y);

    int z = (int)std::floor(current.z);

    if (world.isSolid(x, y, z)) {
      if (place) {
        world.setBlock((int)std::floor(prev.x), (int)std::floor(prev.y),
                       (int)std::floor(prev.z), BlockType::Stone);
      } else {
        world.setBlock(x, y, z, BlockType::Air);
      }

      break;
    }

    prev = current;
  }
}