#include "Scene.h"

#include "../../core/Setting.h"
#include "../../ecs/systems/PhysicsSystem.h"
#include "../../platform/input/Input.h"
#include "../../renderer/opengl/TextureArray.h"
#include "../../renderer/ui/Crosshair.h"
#include <glad/gl.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

void Scene::init() {

  sky.Init();

  uiShader = std::make_unique<Shader>(
        "assets/shaders/ui.vert",
        "assets/shaders/ui.frag"
    );

    crosshair.init(uiShader.get());
    debugOverlay.init(uiShader.get());


  shader = std::make_unique<Shader>("assets/shaders/block.vert",
                                    "assets/shaders/block.frag");

  atlas = std::make_unique<TextureArray>(
      std::vector<std::string>{
          "assets/textures/grass.png",      // layer 0
          "assets/textures/grass_side.png", // layer 1
          "assets/textures/dirt.png",       // layer 2
          "assets/textures/stone.png",      // layer 3
          "assets/textures/sand.png",       // layer 4
      },
      16, true);

  playerModel = std::make_unique<Model>("assets/models/player.obj",
                                        "assets/models/texture_player.png");

  /*
      SPAWN POSITION
  */

  int spawnX = 0;
  int spawnZ = 0;

  /*
      FORCE GENERATE WORLD
  */

  bool generated = false;

  while (!generated) {

    world.update((float)spawnX, (float)spawnZ);

    SDL_Delay(1);

    generated = true;

    /*
        CHECK ALL CHUNKS
    */

    int centerChunkX = spawnX / Chunk::SIZE;

    int centerChunkZ = spawnZ / Chunk::SIZE;

    for (int x = -Setting::renderDistance; x <= Setting::renderDistance; x++) {

      for (int z = -Setting::renderDistance; z <= Setting::renderDistance;
           z++) {

        Chunk *chunk = world.getChunk(centerChunkX + x, centerChunkZ + z);

        if (chunk == nullptr) {

          generated = false;

          break;
        }
      }

      if (!generated)
        break;
    }
  }

  /*
      NOW SAFE TO GET HEIGHT
  */

  int groundY = world.getHeight(spawnX, spawnZ);

  /*
      PLAYER SPAWN
  */

  playerTransform.position = glm::vec3(spawnX, groundY + 5.0f, spawnZ);

  /*
      CAMERA
  */

  camera.position = playerTransform.position;
}

void Scene::update(float dt, SDL_Window *window) {

  time.update(dt);

  fps = (dt > 0.0f) ? 1.0f / dt : 0.0f;

  playerController.update(camera, playerTransform, playerRigidbody,
                          cursorLocked, window, time, world);

  world.update(playerTransform.position.x, playerTransform.position.z);

  PhysicsSystem::update(playerTransform, playerRigidbody, world, dt);
}

void Scene::render() {

  glm::vec3 top = time.getSkyTopColor();

  glm::vec3 hor = time.getSkyHorizonColor();

  glm::vec3 bot = time.getSkyBottomColor();

  glEnable(GL_DEPTH_TEST);

  glDepthMask(GL_TRUE);

  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glm::mat4 projection = glm::perspective(
      glm::radians(Setting::fov),
      (float)Setting::windowWidth / (float)Setting::windowHeight,
      Setting::nearPlane, Setting::farPlane);

  glm::mat4 view = camera.getViewMatrix();

  glm::mat4 invProj = glm::inverse(projection);

  glm::mat4 rotationView = glm::mat4(glm::mat3(view));

  glm::mat4 invView = glm::inverse(rotationView);

  /*
      SKY
  */

  sky.shader->use();

  glUniformMatrix4fv(glGetUniformLocation(sky.shader->id, "invProj"), 1,
                     GL_FALSE, glm::value_ptr(invProj));

  glUniformMatrix4fv(glGetUniformLocation(sky.shader->id, "invView"), 1,
                     GL_FALSE, glm::value_ptr(invView));

  sky.Render(top, hor, bot);

  /*
      WORLD
  */

  frustum.update(projection, view);

  shader->use();

  glUniform3f(glGetUniformLocation(shader->id, "cameraPos"),
              playerTransform.position.x, playerTransform.position.y,
              playerTransform.position.z);

  glUniform3f(glGetUniformLocation(shader->id, "fogColor"), hor.r, hor.g,
              hor.b);

  glUniform1f(glGetUniformLocation(shader->id, "fogStart"), Setting::fogStart);

  glUniform1f(glGetUniformLocation(shader->id, "fogEnd"), Setting::fogEnd);

  glm::mat4 model = glm::mat4(1.0f);

  glUniformMatrix4fv(glGetUniformLocation(shader->id, "model"), 1, GL_FALSE,
                     glm::value_ptr(model));

  glUniformMatrix4fv(glGetUniformLocation(shader->id, "view"), 1, GL_FALSE,
                     glm::value_ptr(view));

  glUniformMatrix4fv(glGetUniformLocation(shader->id, "projection"), 1,
                     GL_FALSE, glm::value_ptr(projection));

  atlas->bind(0);

  shader->setInt("atlas", 0);
  
  glEnable(GL_DEPTH_TEST);

  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  glFrontFace(GL_CCW);

  world.draw(
    playerTransform.position.x,
    playerTransform.position.z,
    frustum
);

  /*
      PLAYER MODEL
  */

  glm::mat4 playerMatrix =
      glm::translate(glm::mat4(1.0f), playerTransform.position);

  glUniformMatrix4fv(glGetUniformLocation(shader->id, "model"), 1, GL_FALSE,
                     glm::value_ptr(playerMatrix));

  playerModel->draw(*shader);

  /*
      CROSSHAIR
  */

  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);

  crosshair.render(Setting::windowWidth, Setting::windowHeight);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);

  int groundY = world.getHeight(
        (int)playerTransform.position.x,
        (int)playerTransform.position.z
    );

    debugOverlay.render(
        Setting::windowWidth,
        Setting::windowHeight,
        fps,
        playerTransform.position,
        camera.front,
        groundY,
        playerController.debugVisible
    );
}