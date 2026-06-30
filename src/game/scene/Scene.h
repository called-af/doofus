#pragma once

#include <glad/gl.h>
#include <memory>

#include "../../camera/Camera.h"
#include "../../core/Time.h"
#include "../../ecs/components/RigidBody.h"
#include "../../ecs/components/Transform.h"
#include "../../platform/sdl/SDLWindow.h"
#include "../../player/PlayerController.h"
#include "../../renderer/Frustum.h"
#include "../../renderer/Sky.h"
#include "../../renderer/model/Model.h"
#include "../../renderer/opengl/Shader.h"
#include "../../renderer/opengl/TextureArray.h"
#include "../../renderer/ui/Crosshair.h"
#include "../../renderer/ui/DebugOverlay.h"
#include "../../world/World.h"

class Scene {
public:
  void init();
  void update(float dt, SDL_Window *window);
  void render();
  bool isLoading = true;

private:
  Camera camera;
  World world;
  Time time;
  Frustum frustum;
  Sky sky;
  Crosshair crosshair;
  DebugOverlay debugOverlay;

  std::unique_ptr<Shader> shader;
  std::unique_ptr<Shader> uiShader;
  std::unique_ptr<Shader> shadowShader;
  std::unique_ptr<TextureArray> atlas;
  std::unique_ptr<Model> playerModel;
  std::unique_ptr<Shader> playerShader;

  PlayerController playerController;
  TransformComponent playerTransform;
  RigidbodyComponent playerRigidbody;

  bool cursorLocked = true;
  float fps = 0.0f;

  // Shadow mapping
  GLuint shadowFBO = 0;
  GLuint shadowDepthTexture = 0;
  static constexpr int SHADOW_WIDTH = 1024;
  static constexpr int SHADOW_HEIGHT = 1024;
  glm::mat4 lightSpaceMatrix = glm::mat4(1.0f);

  void setupShadowPass();
  void renderShadowPass();
};