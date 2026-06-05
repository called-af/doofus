#pragma once

#include "../../camera/Camera.h"
#include "../../world/World.h"
#include "../../renderer/Sky.h"
#include "../../renderer/opengl/Shader.h"
#include "../../renderer/opengl/TextureArray.h"
#include "../../renderer/ui/Crosshair.h"
#include "../../renderer/ui/DebugOverlay.h"
#include "../../renderer/Frustum.h"
#include "../../ecs/components/RigidBody.h"
#include "../../ecs/components/Transform.h"
#include "../../player/PlayerController.h"
#include "../../renderer/model/Model.h"
#include "../../core/Time.h"
#include "../../platform/sdl/SDLWindow.h"

#include <memory>

class Scene {
public:
    void init();
    void update(float dt, SDL_Window* window);
    void render();

private:
    Camera           camera;
    World            world;
    Time             time;
    Frustum          frustum;
    Sky              sky;
    Crosshair        crosshair;
    DebugOverlay     debugOverlay;

    std::unique_ptr<Shader>       shader;       // block shader
    std::unique_ptr<Shader>       uiShader;     // shared UI shader
    std::unique_ptr<TextureArray> atlas;
    std::unique_ptr<Model>        playerModel;

    PlayerController   playerController;
    TransformComponent playerTransform;
    RigidbodyComponent playerRigidbody;

    bool cursorLocked = true;
    float fps = 0.0f;      
};