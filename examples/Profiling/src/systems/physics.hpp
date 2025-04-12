#pragma once

#include <Freyr/Freyr.hpp>

#include "../events/collision.hpp"

#include <print>

class PhysicsSystem final : public fr::System
{
  public:
    explicit PhysicsSystem(const Ref<fr::Scene>& scene) :
        System(scene), count(0)
    {
        mScene->AddEventListener<CollisionEvent>(
            [&](CollisionEvent collisionEvent) { count++; });
    }

    ~PhysicsSystem() override = default;

    void Update(float deltaTime) override { std::println("count: {}", count); }

    int count;
};