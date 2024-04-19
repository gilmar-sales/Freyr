#pragma once

#include "Freyr.hpp"

#include "../events/collision.hpp"

class PhysicsSystem : public fr::System
{
  public:
    void Start() override
    {
        mManager->AddEventListener<CollisionEvent>([](CollisionEvent collisionEvent) {
        });
    }

    void Update(float deltaTime) override
    {
    }
};