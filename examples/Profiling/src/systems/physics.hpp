#pragma once

#include <print>

#include "Freyr.hpp"

#include "../events/collision.hpp"

class PhysicsSystem : public fr::System
{
    public:

    void Start() override
    {
        mManager->AddEventListener<CollisionEvent>([](CollisionEvent collisionEvent)
        {
            std::println("Collision received");
        });
    }

    void Update(float deltaTime) override
    {
    }
};