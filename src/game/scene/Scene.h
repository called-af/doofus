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
  int shadowMapRes = 0;
  bool shadowActive = false;   // Shadow state hysteresis flag shared between renderShadowPass and render to prevent flicker
  glm::mat4 lightSpaceMatrix = glm::mat4(1.0f);

  // Cached uniform locations to eliminate glGetUniformLocation per frame
  struct BlockShaderUniforms {
    GLint cameraPos = -1;
    GLint fogColor = -1;
    GLint fogStart = -1;
    GLint fogEnd = -1;
    GLint uTopColor = -1;
    GLint uLightDir = -1;
    GLint uLightColor = -1;
    GLint uAmbientColor = -1;
    GLint uShadowDistance = -1;
    GLint uShadowsEnabled = -1;
    GLint model = -1;
    GLint view = -1;
    GLint projection = -1;
    GLint lightSpaceMatrix = -1;
  } blockUniforms;

  struct ShadowShaderUniforms {
    GLint lightSpaceMatrix = -1;
    GLint model = -1;
    GLint uTime = -1;
    GLint uIsLOD = -1;
    GLint uLodSpawnTime = -1;
  } shadowUniforms;

  struct SkyShaderUniforms {
    GLint invProj = -1;
    GLint invView = -1;
  } skyUniforms;

  struct PlayerShaderUniforms {
    GLint model = -1;
    GLint view = -1;
    GLint projection = -1;
  } playerUniforms;

  void setupShadowPass();
  void renderShadowPass();
};