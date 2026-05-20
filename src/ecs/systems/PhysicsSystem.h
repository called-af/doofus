#pragma once

#include "../components/Transform.h"
#include "../components/RigidBody.h"

#include "../../renderer/world/World.h"

class PhysicsSystem
{
public:
    static void update(
        TransformComponent& transform,
        RigidbodyComponent& rigidbody,
        World& world,
        float dt
    );
};