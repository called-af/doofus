#include "PhysicsSystem.h"

void PhysicsSystem::update(
    TransformComponent& transform,
    RigidbodyComponent& rigidbody,
    World& world,
    float dt)
{
    rigidbody.velocity.y += rigidbody.gravity * dt;

    glm::vec3 next = transform.position + rigidbody.velocity * dt;

    float size = 0.3f;

    bool hit = false;

    for (float x = -size; x <= size; x += size)
    for (float y = 0; y <= 1.8f; y += 0.9f)
    for (float z = -size; z <= size; z += size)
    {
        if (world.isSolid(
            (int)floor(next.x + x),
            (int)floor(next.y + y),
            (int)floor(next.z + z)))
        {
            hit = true;
        }
    }

    if (hit)
    {
        rigidbody.velocity.y = 0;
        rigidbody.grounded = true;
        next.y = floor(next.y) + 1.0f;
    }
    else
    {
        rigidbody.grounded = false;
    }

    transform.position = next;

    rigidbody.velocity.x *= 0.82f;
    rigidbody.velocity.z *= 0.82f;
}