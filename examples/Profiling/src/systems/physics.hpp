#pragma once

#include <Freyr/Freyr.hpp>

#include "../components/position.hpp"
#include "../components/velocity.hpp"
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

    void PreUpdate(float deltaTime, const Ref<fr::Scheduler>& scheduler) override
    {
        scheduler->Run<Position, Velocity>(
            "PhysicsUpdate", [deltaTime](fr::Entity entity, Position& position, const Velocity& velocity) {
                position.x += velocity.x * deltaTime;
            });

        scheduler->Run<Position>("PositionUpdate", [](fr::Entity entity, Position& position) { position.y += 1; });
    }

    void Update(float deltaTime, const Ref<fr::Scheduler>& scheduler) override {}
};