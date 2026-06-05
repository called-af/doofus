#pragma once

#include <SDL3/SDL.h>

#include "../camera/Camera.h"
#include "../core/Setting.h"

#include "../ecs/components/RigidBody.h"
#include "../ecs/components/Transform.h"
class Time;
class World;

class PlayerController {
public:
  void update(Camera &camera, TransformComponent &transform,
              RigidbodyComponent &rigidbody, bool &cursorLocked,
              SDL_Window *window, Time &time, World &world);

public:
  bool debugVisible = false;
  
private:
  void raycast(Camera &camera, World &world, bool place);

private:
  float reach = Setting::reachDistance;

  int breakCooldown = Setting::breakCooldown;
  int placeCooldown = Setting::placeCooldown;

  bool bHeld = false;

  int lastBreakTick = 0;
  int lastPlaceTick = 0;
};