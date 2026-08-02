#pragma once

#include <Freyr/Freyr.hpp>

#include "../components/position.hpp"
#include "../components/velocity.hpp"
#include "../events/collision.hpp"

inline thread_local std::atomic<int> mCollisionCount = 0;

class PhysicsSystem final : public fr::System
{
  public:
    explicit PhysicsSystem(const skr::Arc<fr::Registry>& registry) : System(registry)
    {
        mCollisionHandle =
            mRegistry->AddEventListener<CollisionEvent>([&](const CollisionEvent& collisionEvent) { ++mCollisionCount; });
    }

    ~PhysicsSystem() override = default;

    void PreUpdate(float deltaTime) override
    {
        mRegistry->CreateMutation()->EachAsync<Position, Velocity>([deltaTime](Position& position, const Velocity& velocity) {
            position.x += velocity.x * deltaTime;
        });

        mRegistry->CreateMutation()->WithLabel("UpdatePosition").EachAsync<Position>([](Position& position) {
            position.y += 1;
        });
    }

    void Update(float deltaTime) override {}

  private:
    skr::Arc<fr::ListenerHandle> mCollisionHandle;
};