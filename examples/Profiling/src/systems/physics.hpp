#pragma once

#include <Freyr/Freyr.hpp>

#include "../events/collision.hpp"

inline thread_local std::atomic<int> mCollisionCount = 0;

class PhysicsSystem final : public fr::System
{
  public:
    explicit PhysicsSystem(const std::shared_ptr<fr::Scene>& scene) : System(scene)
    {
        mScene->AddEventListener<CollisionEvent>([&](CollisionEvent collisionEvent) { ++mCollisionCount; });
    }

    ~PhysicsSystem() override = default;

    void PreUpdate(float deltaTime) override
    {
        mScene->CreateQuery()->EachAsync<Position, Velocity>([deltaTime](Position& position, const Velocity& velocity) {
            position.x += velocity.x * deltaTime;
        });

        mScene->CreateQuery()->EachAsync<Position>([](Position& position) { position.y += 1; });
    }

    void Update(float deltaTime) override {}
};