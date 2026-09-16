#include "Scene.h"

#include "../../core/Setting.h"
#include "../../ecs/systems/PhysicsSystem.h"
#include "../../platform/input/Input.h"
#include "../../renderer/PointLight.h"
#include "../../renderer/opengl/TextureArray.h"
#include "../../renderer/ui/Crosshair.h"
#include "../../world/biome/BiomeManager.h"
#include "../../world/climate/ClimateSampler.h"
#include "../../world/terrain/TerrainSampler.h"
#include <glad/gl.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <iostream>

void Scene::init()
{

    sky.Init();

    uiShader = std::make_unique<Shader>("assets/shaders/ui.vert",
                                        "assets/shaders/ui.frag");

    crosshair.init(uiShader.get());
    debugOverlay.init(uiShader.get());

    shader = std::make_unique<Shader>("assets/shaders/block.vert",
                                      "assets/shaders/block.frag");

    atlas = std::make_unique<TextureArray>(
        std::vector<std::string>{
            "assets/textures/grass.png",        // Layer 0
            "assets/textures/grass_side.png",   // Layer 1
            "assets/textures/dirt.png",         // Layer 2
            "assets/textures/stone.png",        // Layer 3
            "assets/textures/sand.png",         // Layer 4
            "assets/textures/basalt.png",       // Layer 5
            "assets/textures/lava.png",         // Layer 6
            "assets/textures/obsidian.png",     // Layer 7
            "assets/textures/ash.png",          // Layer 8
            "assets/textures/cinder.png",       // Layer 9
        },
        16, true);

    playerShader = std::make_unique<Shader>("assets/shaders/model.vert",
                                            "assets/shaders/model.frag");

    playerModel = std::make_unique<Model>("assets/models/player.obj",
                                          "assets/models/texture.png");

    // Pre-cache all shader uniform locations (eliminates glGetUniformLocation queries every frame)
    blockUniforms.cameraPos = glGetUniformLocation(shader->id, "cameraPos");
    blockUniforms.fogColor = glGetUniformLocation(shader->id, "fogColor");
    blockUniforms.fogStart = glGetUniformLocation(shader->id, "fogStart");
    blockUniforms.fogEnd = glGetUniformLocation(shader->id, "fogEnd");
    blockUniforms.uTopColor = glGetUniformLocation(shader->id, "uTopColor");
    blockUniforms.uLightDir = glGetUniformLocation(shader->id, "uLightDir");
    blockUniforms.uLightColor = glGetUniformLocation(shader->id, "uLightColor");
    blockUniforms.uAmbientColor = glGetUniformLocation(shader->id, "uAmbientColor");
    blockUniforms.model = glGetUniformLocation(shader->id, "model");
    blockUniforms.view = glGetUniformLocation(shader->id, "view");
    blockUniforms.projection = glGetUniformLocation(shader->id, "projection");
    blockUniforms.uTime = glGetUniformLocation(shader->id, "uTime");
    blockUniforms.uNumPointLights = glGetUniformLocation(shader->id, "uNumPointLights");
    for (int i = 0; i < LightRegistry::MAX_LIGHTS; ++i) {
        char buf[64];
        snprintf(buf, sizeof(buf), "uPointLights[%d].position",  i); blockUniforms.uPointLightPos[i]       = glGetUniformLocation(shader->id, buf);
        snprintf(buf, sizeof(buf), "uPointLights[%d].color",     i); blockUniforms.uPointLightColor[i]     = glGetUniformLocation(shader->id, buf);
        snprintf(buf, sizeof(buf), "uPointLights[%d].radius",    i); blockUniforms.uPointLightRadius[i]    = glGetUniformLocation(shader->id, buf);
        snprintf(buf, sizeof(buf), "uPointLights[%d].intensity", i); blockUniforms.uPointLightIntensity[i] = glGetUniformLocation(shader->id, buf);
    }

    skyUniforms.invProj = glGetUniformLocation(sky.shader->id, "invProj");
    skyUniforms.invView = glGetUniformLocation(sky.shader->id, "invView");

    playerUniforms.model = glGetUniformLocation(playerShader->id, "model");
    playerUniforms.view = glGetUniformLocation(playerShader->id, "view");
    playerUniforms.projection = glGetUniformLocation(playerShader->id, "projection");
    playerUniforms.cameraPos = glGetUniformLocation(playerShader->id, "cameraPos");
    playerUniforms.fogColor = glGetUniformLocation(playerShader->id, "fogColor");
    playerUniforms.fogStart = glGetUniformLocation(playerShader->id, "fogStart");
    playerUniforms.fogEnd = glGetUniformLocation(playerShader->id, "fogEnd");
    playerUniforms.uLightDir = glGetUniformLocation(playerShader->id, "uLightDir");
    playerUniforms.uLightColor = glGetUniformLocation(playerShader->id, "uLightColor");
    playerUniforms.uAmbientColor = glGetUniformLocation(playerShader->id, "uAmbientColor");
    playerUniforms.uNumPointLights = glGetUniformLocation(playerShader->id, "uNumPointLights");
    for (int i = 0; i < LightRegistry::MAX_LIGHTS; ++i) {
        char buf[64];
        snprintf(buf, sizeof(buf), "uPointLights[%d].position",  i); playerUniforms.uPointLightPos[i]       = glGetUniformLocation(playerShader->id, buf);
        snprintf(buf, sizeof(buf), "uPointLights[%d].color",     i); playerUniforms.uPointLightColor[i]     = glGetUniformLocation(playerShader->id, buf);
        snprintf(buf, sizeof(buf), "uPointLights[%d].radius",    i); playerUniforms.uPointLightRadius[i]    = glGetUniformLocation(playerShader->id, buf);
        snprintf(buf, sizeof(buf), "uPointLights[%d].intensity", i); playerUniforms.uPointLightIntensity[i] = glGetUniformLocation(playerShader->id, buf);
    }

    /*
        CAMERA
    */

    camera.position = playerTransform.position;

    glm::mat4 projection = glm::perspective(
        glm::radians(camera.currentFov),
        (float)Setting::windowWidth / (float)Setting::windowHeight,
        Setting::nearPlane, Setting::farPlane);
    glm::mat4 view = camera.getViewMatrix();
    frustum.update(projection, view);

    // Pass TRUE as last argument (still in init/loading phase)
    world.update(camera.position, camera.front, frustum, true);
}

void Scene::update(float dt, SDL_Window *window)
{
    if (isLoading)
    {
        glm::mat4 projection = glm::perspective(
            glm::radians(camera.currentFov),
            (float)Setting::windowWidth / (float)Setting::windowHeight,
            Setting::nearPlane, Setting::farPlane);
        glm::mat4 view = camera.getViewMatrix();
        frustum.update(projection, view);

        // Pass TRUE as last argument (currently in loading state)
        world.update(camera.position, camera.front, frustum, true);

        bool allReady = true;
        for (int x = -3; x <= 3 && allReady; x++)
        {
            for (int z = -3; z <= 3 && allReady; z++)
            {
                Chunk *chunk = world.getChunk(x, z);
                if (!chunk || chunk->dirty || !chunk->mesh)
                    allReady = false;
            }
        }

        if (allReady)
        {
            int groundY = world.getHeight(0, 0);
            playerTransform.position = glm::vec3(0, groundY + 5.0f, 0);
            camera.position = playerTransform.position;
            isLoading = false;
        }
        return;
    }

    time.update(dt);
    fps = (dt > 0.0f) ? 1.0f / dt : 0.0f;

    playerController.update(camera, playerTransform, playerRigidbody,
                            cursorLocked, window, time, world, dt);

    glm::mat4 projection = glm::perspective(
        glm::radians(camera.currentFov),
        (float)Setting::windowWidth / (float)Setting::windowHeight,
        Setting::nearPlane, Setting::farPlane);
    glm::mat4 view = camera.getViewMatrix();
    frustum.update(projection, view);

    // Pass FALSE as last argument (gameplay has started)
    world.update(camera.position, camera.front, frustum, false);

    PhysicsSystem::update(playerTransform, playerRigidbody, world, dt);
}

void Scene::render()
{

    if (isLoading)
    {
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        // Optional: render loading screen text here
        return;
    }

    glm::vec3 top = time.getSkyTopColor();

    glm::vec3 hor = time.getSkyHorizonColor();

    glm::vec3 bot = time.getSkyBottomColor();

    glm::vec3 sunDir = time.getSunDirection();

    glm::vec3 moonDir = time.getMoonDirection();

    glEnable(GL_DEPTH_TEST);

    glDepthMask(GL_TRUE);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glm::mat4 projection = glm::perspective(
        glm::radians(camera.currentFov),
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

    glUniformMatrix4fv(skyUniforms.invProj, 1, GL_FALSE, glm::value_ptr(invProj));
    glUniformMatrix4fv(skyUniforms.invView, 1, GL_FALSE, glm::value_ptr(invView));

    sky.Render(top, hor, bot, sunDir, moonDir);

    /*
      WORLD (inside Scene::render())
    */
    // Calculate active light parameters
    float progress = time.getDayProgress();
    auto getSunBrightness = [](float p)
    {
        float angle = (p - 0.25f) * 2.0f * 3.141592f;
        return glm::clamp(std::cos(angle) * 1.3f, 0.0f, 1.0f);
    };
    float sunIntensity = getSunBrightness(progress);
    float moonProgress = progress + 0.5f;
    if (moonProgress > 1.0f)
        moonProgress -= 1.0f;
    float moonIntensity = getSunBrightness(moonProgress);

    float sunsetFactor = 1.0f - std::abs(sunIntensity - 0.5f) * 2.0f;
    sunsetFactor = glm::clamp(sunsetFactor, 0.0f, 1.0f);

    // Vibrant daylight color (warm gold-white) and beautiful deep sunset orange
    glm::vec3 dayColor = glm::mix(glm::vec3(0.76f, 0.70f, 0.60f), glm::vec3(0.72f, 0.32f, 0.10f), sunsetFactor);
    glm::vec3 sunLightColor = dayColor * sunIntensity;
    // Clear, rich silver-blue moonlight
    glm::vec3 moonLightColor = glm::vec3(0.14f, 0.22f, 0.38f) * moonIntensity;

    glm::vec3 activeLightDir;
    glm::vec3 activeLightColor;
    if (sunDir.y >= 0.0f)
    {
        activeLightDir = sunDir;
        activeLightColor = sunLightColor;
    }
    else
    {
        activeLightDir = moonDir;
        activeLightColor = moonLightColor;
    }

    // Deeply saturated sky-blue ambient light during the day, and dark purple-blue at night
    glm::vec3 dayAmbient = glm::vec3(0.24f, 0.28f, 0.38f) * (0.4f + 0.6f * sunIntensity);
    glm::vec3 nightAmbient = glm::vec3(0.03f, 0.04f, 0.07f) * (0.5f + 0.5f * moonIntensity);
    glm::vec3 activeAmbientColor = glm::mix(nightAmbient, dayAmbient, sunIntensity);

    frustum.update(projection, view);
    shader->use();

    glUniform3f(blockUniforms.cameraPos,
                playerTransform.position.x, playerTransform.position.y,
                playerTransform.position.z);

    glUniform3f(blockUniforms.fogColor, hor.r, hor.g, hor.b);
    glUniform1f(blockUniforms.fogStart, Setting::getFogStart());
    glUniform1f(blockUniforms.fogEnd, Setting::getFogEnd());
    glUniform3f(blockUniforms.uTopColor, top.r, top.g, top.b);

    glUniform3f(blockUniforms.uLightDir, activeLightDir.x, activeLightDir.y, activeLightDir.z);
    glUniform3f(blockUniforms.uLightColor, activeLightColor.r, activeLightColor.g, activeLightColor.b);
    glUniform3f(blockUniforms.uAmbientColor, activeAmbientColor.r, activeAmbientColor.g, activeAmbientColor.b);

    glm::mat4 model = glm::mat4(1.0f);
    glUniformMatrix4fv(blockUniforms.model, 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(blockUniforms.view, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(blockUniforms.projection, 1, GL_FALSE, glm::value_ptr(projection));
    glUniform1f(blockUniforms.uTime, (float)(SDL_GetTicks() / 1000.0));

    // ── Upload registered point lights ────────────────────────────────────
    const auto& lights = LightRegistry::getAll();
    int numLights = LightRegistry::count();
    glUniform1i(blockUniforms.uNumPointLights, numLights);
    for (int i = 0; i < numLights; ++i) {
        glUniform3fv(blockUniforms.uPointLightPos[i],   1, &lights[i].position.x);
        glUniform3fv(blockUniforms.uPointLightColor[i], 1, &lights[i].color.x);
        glUniform1f (blockUniforms.uPointLightRadius[i],    lights[i].radius);
        glUniform1f (blockUniforms.uPointLightIntensity[i], lights[i].intensity);
    }

    // Bind atlas to unit 0
    glActiveTexture(GL_TEXTURE0);
    atlas->bind(0);
    shader->setInt("atlas", 0);

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    // Terrain is opaque. Keeping blending disabled ensures maximum early depth rejection.
    glDisable(GL_BLEND);

    world.draw(camera.position, camera.front, frustum, projection * view, shader->id);
    world.drawLOD(camera.position, frustum, projection * view, shader->id);

    /*
        PLAYER MODEL
    */

    if (camera.mode != CameraMode::FirstPerson && playerModel)
    {
        playerShader->use();

        // Scale Blockbench model (77.16 units height) to 1.85 blocks tall
        constexpr float modelScale = 1.85f / 77.16f;

        glm::mat4 playerMatrix = glm::translate(glm::mat4(1.0f), playerTransform.position);
        // camera.yaw = -90 is forward (-Z). Rotate player model to face camera look direction
        playerMatrix = glm::rotate(playerMatrix, glm::radians(-camera.yaw - 90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        playerMatrix = glm::scale(playerMatrix, glm::vec3(modelScale));

        glUniformMatrix4fv(playerUniforms.model, 1, GL_FALSE, glm::value_ptr(playerMatrix));
        glUniformMatrix4fv(playerUniforms.view, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(playerUniforms.projection, 1, GL_FALSE, glm::value_ptr(projection));

        glUniform3f(playerUniforms.cameraPos, camera.position.x, camera.position.y, camera.position.z);
        glUniform3f(playerUniforms.fogColor, hor.r, hor.g, hor.b);
        glUniform1f(playerUniforms.fogStart, Setting::getFogStart());
        glUniform1f(playerUniforms.fogEnd, Setting::getFogEnd());

        glUniform3f(playerUniforms.uLightDir, activeLightDir.x, activeLightDir.y, activeLightDir.z);
        glUniform3f(playerUniforms.uLightColor, activeLightColor.r, activeLightColor.g, activeLightColor.b);
        glUniform3f(playerUniforms.uAmbientColor, activeAmbientColor.r, activeAmbientColor.g, activeAmbientColor.b);

        // ── Point lights ──────────────────────────────────────────────────
        const auto& plights = LightRegistry::getAll();
        int npl = LightRegistry::count();
        glUniform1i(playerUniforms.uNumPointLights, npl);
        for (int i = 0; i < npl; ++i) {
            glUniform3fv(playerUniforms.uPointLightPos[i],   1, &plights[i].position.x);
            glUniform3fv(playerUniforms.uPointLightColor[i], 1, &plights[i].color.x);
            glUniform1f (playerUniforms.uPointLightRadius[i],    plights[i].radius);
            glUniform1f (playerUniforms.uPointLightIntensity[i], plights[i].intensity);
        }

        glDisable(GL_CULL_FACE);

        playerModel->draw(*playerShader);

        glEnable(GL_CULL_FACE);
    }

    /*
        CROSSHAIR
    */

    if (camera.mode == CameraMode::FirstPerson || camera.mode == CameraMode::ThirdPersonBack)
    {
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);

        crosshair.render(Setting::windowWidth, Setting::windowHeight);

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
    }

    if (playerController.debugVisible)
    {
        int groundY = world.getHeight((int)playerTransform.position.x,
                                      (int)playerTransform.position.z);

        TerrainSample terrain = TerrainSampler::sample(
            (int)playerTransform.position.x, (int)playerTransform.position.z);

        ClimateSample climate = ClimateSampler::sample(
            (int)playerTransform.position.x, (int)playerTransform.position.z);

        Biome *biome = BiomeManager::getBiome(
            terrain, climate,
            (int)playerTransform.position.x, (int)playerTransform.position.z,
            (int)playerTransform.position.y);

        debugOverlay.render(Setting::windowWidth, Setting::windowHeight, fps,
                            playerTransform.position, camera.front, groundY,
                            biome->getName(), camera.mode, playerController.debugVisible);
    }
}
