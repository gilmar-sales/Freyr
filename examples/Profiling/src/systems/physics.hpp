#pragma once

#include <Freyr/Freyr.hpp>

#include "../events/collision.hpp"

#include <print>
class PhysicsSystem : public fr::System
{
  public:
    void Start() override
    {
        mManager->AddEventListener<CollisionEvent>(
            [&](CollisionEvent collisionEvent) { count++; });
    }

    void Update(float deltaTime) override { std::println("count: {}", count); }

    int count = 0;
};