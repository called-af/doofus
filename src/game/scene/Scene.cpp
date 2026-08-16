#include "Scene.h"

#include "../../core/Setting.h"
#include "../../ecs/systems/PhysicsSystem.h"
#include "../../platform/input/Input.h"
#include "../../renderer/opengl/TextureArray.h"
#include "../../renderer/ui/Crosshair.h"
#include "../../world/biome/BiomeManager.h"
#include "../../world/climate/ClimateSampler.h"
#include "../../world/terrain/TerrainSampler.h"
#include <glad/gl.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
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

    shadowShader = std::make_unique<Shader>("assets/shaders/shadow.vert",
                                            "assets/shaders/shadow.frag");

    atlas = std::make_unique<TextureArray>(
        std::vector<std::string>{
            "assets/textures/grass.png",
            "assets/textures/grass_side.png",
            "assets/textures/dirt.png",
            "assets/textures/stone.png",
            "assets/textures/sand.png",
            "assets/textures/basalt.png",
            "assets/textures/obsidian.png",
            "assets/textures/ash.png",
            "assets/textures/cinder.png",
            "assets/textures/lava.png",
            "assets/textures/heaven_stone.png",
            "assets/textures/crystal.png",
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
    blockUniforms.uShadowDistance = glGetUniformLocation(shader->id, "uShadowDistance");
    blockUniforms.uShadowsEnabled = glGetUniformLocation(shader->id, "uShadowsEnabled");
    blockUniforms.model = glGetUniformLocation(shader->id, "model");
    blockUniforms.view = glGetUniformLocation(shader->id, "view");
    blockUniforms.projection = glGetUniformLocation(shader->id, "projection");
    blockUniforms.lightSpaceMatrix = glGetUniformLocation(shader->id, "lightSpaceMatrix");

    shadowUniforms.lightSpaceMatrix = glGetUniformLocation(shadowShader->id, "lightSpaceMatrix");
    shadowUniforms.model = glGetUniformLocation(shadowShader->id, "model");
    shadowUniforms.uTime = glGetUniformLocation(shadowShader->id, "uTime");
    shadowUniforms.uIsLOD = glGetUniformLocation(shadowShader->id, "uIsLOD");
    shadowUniforms.uLodSpawnTime = glGetUniformLocation(shadowShader->id, "uLodSpawnTime");

    skyUniforms.invProj = glGetUniformLocation(sky.shader->id, "invProj");
    skyUniforms.invView = glGetUniformLocation(sky.shader->id, "invView");

    playerUniforms.model = glGetUniformLocation(playerShader->id, "model");
    playerUniforms.view = glGetUniformLocation(playerShader->id, "view");
    playerUniforms.projection = glGetUniformLocation(playerShader->id, "projection");

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

    setupShadowPass();

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

    // Render shadow pass first
    renderShadowPass();

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
    glUniform1f(blockUniforms.uShadowDistance, (float)Setting::shadowDistance * 16.0f);
    glUniform1i(blockUniforms.uShadowsEnabled, (Setting::enableShadows && shadowActive) ? 1 : 0);

    glm::mat4 model = glm::mat4(1.0f);
    glUniformMatrix4fv(blockUniforms.model, 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(blockUniforms.view, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(blockUniforms.projection, 1, GL_FALSE, glm::value_ptr(projection));

    // Pass light space matrix for shadow mapping
    glUniformMatrix4fv(blockUniforms.lightSpaceMatrix, 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));

    // Bind shadow map to texture unit 1
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, shadowDepthTexture);
    shader->setInt("shadowMap", 1);

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

    if (camera.mode != CameraMode::FirstPerson)
    {
        glm::mat4 playerMatrix =
            glm::translate(glm::mat4(1.0f), playerTransform.position);

        playerShader->use();

        glUniformMatrix4fv(playerUniforms.model, 1, GL_FALSE, glm::value_ptr(playerMatrix));
        glUniformMatrix4fv(playerUniforms.view, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(playerUniforms.projection, 1, GL_FALSE, glm::value_ptr(projection));

        playerModel->draw(*playerShader);
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

        Biome *biome = BiomeManager::getBiome(terrain, climate);

        debugOverlay.render(Setting::windowWidth, Setting::windowHeight, fps,
                            playerTransform.position, camera.front, groundY,
                            biome->getName(), camera.mode, playerController.debugVisible);
    }
}

void Scene::setupShadowPass()
{
    int neededRes = Setting::shadowMapSize();

    if (shadowFBO != 0 && shadowMapRes == neededRes)
        return;

    if (shadowDepthTexture != 0)
    {
        glDeleteTextures(1, &shadowDepthTexture);
        shadowDepthTexture = 0;
    }
    if (shadowFBO != 0)
    {
        glDeleteFramebuffers(1, &shadowFBO);
        shadowFBO = 0;
    }

    shadowMapRes = neededRes;

    glGenFramebuffers(1, &shadowFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);

    glGenTextures(1, &shadowDepthTexture);
    glBindTexture(GL_TEXTURE_2D, shadowDepthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F,
                 shadowMapRes, shadowMapRes, 0,
                 GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

    // GL_LINEAR required for PCF — hardware interpolates between
    // 4 depth samples when sampling between texels → soft edge shadow.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                           GL_TEXTURE_2D, shadowDepthTexture, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "Shadow framebuffer not complete!" << std::endl;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Scene::renderShadowPass()
{
    glm::vec3 sunDir = time.getSunDirection();
    glm::vec3 moonDir = time.getMoonDirection();
    glm::vec3 lightDir = (sunDir.y >= 0.0f) ? sunDir : moonDir;

    // ── Hysteresis: prevent shadow flicker on/off at sunset/sunrise ──────────
    if (shadowActive)
    {
        if (lightDir.y < 0.02f)
            shadowActive = false;
    }
    else
    {
        if (lightDir.y >= 0.05f)
            shadowActive = true;
    }

    if (!shadowActive || !Setting::enableShadows)
    {
        setupShadowPass();
        glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
        glViewport(0, 0, shadowMapRes, shadowMapRes);
        glClear(GL_DEPTH_BUFFER_BIT);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, Setting::windowWidth, Setting::windowHeight);
        lightSpaceMatrix = glm::mat4(1.0f);
        return;
    }

    setupShadowPass();

    // ── Ortho radius — world area covered by shadow ────────────────────────────
    const float shadowR = Setting::shadowDistance * 16.0f;

    // ── Up vector ─────────────────────────────────────────────────────────────
    glm::vec3 up = (std::abs(lightDir.y) > 0.98f)
                       ? glm::vec3(0.0f, 0.0f, 1.0f)
                       : glm::vec3(0.0f, 1.0f, 0.0f);

    // ── Correct texel snapping ─────────────────────────────────────────────
    //
    // worldTexel = size of 1 shadow map texel in world units
    //   = (shadowR * 2) / shadowMapRes
    //
    // Snapping method:
    //  1. Compute light-space right & up vectors (from cross product)
    //  2. Project playerPos onto both axes
    //  3. Snap projection to worldTexel grid
    //  4. Reconstruct world-space center:
    //     snappedCenter = playerPos
    //                   - (frac right offset) * lightRight
    //                   - (frac up   offset) * lightUp
    //
    // This does NOT discard player position components — we only correct
    // the sub-texel fractional offset, not replacing the position entirely.
    //
    const float worldTexel = (shadowR * 2.0f) / (float)shadowMapRes;

    glm::vec3 lightRight = glm::normalize(glm::cross(up, lightDir));
    glm::vec3 lightUp = glm::normalize(glm::cross(lightDir, lightRight));

    glm::vec3 playerPos = playerTransform.position;

    // Project onto light-space axes
    float projR = glm::dot(playerPos, lightRight);
    float projU = glm::dot(playerPos, lightUp);

    // Extract fractional offset only (remainder after snapping to grid)
    float fracR = projR - std::floor(projR / worldTexel) * worldTexel;
    float fracU = projU - std::floor(projU / worldTexel) * worldTexel;

    // Correction: shift playerPos back by fractional amount → shadow grid locks in
    glm::vec3 snappedCenter = playerPos - lightRight * fracR - lightUp * fracU;

    // ── Light view matrix ─────────────────────────────────────────────────────
    // Eye position far along light direction, target = snappedCenter
    glm::mat4 lightView = glm::lookAt(
        snappedCenter + lightDir * 512.0f,
        snappedCenter,
        up);

    // ── Light ortho projection ────────────────────────────────────────────────
    // near/far must cover the entire world:
    //   - World height max = 256
    //   - Light eye = snappedCenter + lightDir*512
    //   - Farthest fragment from eye ≈ 512 + 256 + buffer
    //   - Use near=0.1, far=1024 to be safe at all light angles
    glm::mat4 lightProjection = glm::ortho(
        -shadowR, shadowR,
        -shadowR, shadowR,
        0.1f, 1024.0f);

    lightSpaceMatrix = lightProjection * lightView;

    // ── Render to shadow FBO ──────────────────────────────────────────────────
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
    glViewport(0, 0, shadowMapRes, shadowMapRes);
    glClear(GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);

    // Do NOT cull faces in shadow pass for voxel geometry.
    // Greedy mesher only emits visible faces — no solid back faces exist.
    // Using GL_FRONT discards all faces → empty shadow map.
    glDisable(GL_CULL_FACE);

    shadowShader->use();
    glUniformMatrix4fv(shadowUniforms.lightSpaceMatrix, 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));

    glm::mat4 model = glm::mat4(1.0f);
    glUniformMatrix4fv(shadowUniforms.model, 1, GL_FALSE, glm::value_ptr(model));

    float nowSec = (float)(SDL_GetTicks() / 1000.0);
    glUniform1f(shadowUniforms.uTime, nowSec);
    glUniform1i(shadowUniforms.uIsLOD, 0);

    Frustum lightFrustum;
    lightFrustum.update(lightProjection, lightView);

    int playerChunkX = (int)std::floor(playerPos.x / Chunk::SIZE);
    int playerChunkZ = (int)std::floor(playerPos.z / Chunk::SIZE);

    world.drawShadowChunks(lightFrustum, shadowUniforms.uLodSpawnTime, playerChunkX, playerChunkZ, Setting::shadowDistance);

    glEnable(GL_CULL_FACE); // restore
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, Setting::windowWidth, Setting::windowHeight);
}
