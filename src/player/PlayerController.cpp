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
                              SDL_Window *window, Time &time, World &world) {

  /*
      CURSOR TOGGLE
  */

  if (Input::escapePressed) {
    cursorLocked = !cursorLocked;

    SDL_SetWindowRelativeMouseMode(window, cursorLocked);
  }
   if (Input::bPressed)
    debugVisible = !debugVisible;

  /*
      CAMERA
  */

  if (cursorLocked) {
    camera.yaw += Input::mouseX * Setting::mouseSensitivity;

    camera.pitch -= Input::mouseY * Setting::mouseSensitivity;

    camera.pitch = glm::clamp(camera.pitch, -89.0f, 89.0f);

    camera.updateVectors();
  }

  /*
      MOVEMENT
  */

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
      BLOCK INTERACTION
  */

  if (Input::left_click) {
    if (time.ticks - lastBreakTick >= breakCooldown) {
      raycast(camera, world, false);

      lastBreakTick = time.ticks;
    }
  }

  if (Input::right_click) {
    if (time.ticks - lastPlaceTick >= placeCooldown) {
      raycast(camera, world, true);

      lastPlaceTick = time.ticks;
    }
  }

  /*
      CAMERA FOLLOW
  */

  camera.position =
      transform.position + glm::vec3(0.0f, Setting::cameraEyeHeight, 0.0f);
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