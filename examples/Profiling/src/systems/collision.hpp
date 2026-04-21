#pragma once

#include <Freyr/Freyr.hpp>

#include "../components/position.hpp"
#include "../events/collision.hpp"

class CollisionSystem final : public fr::System
{
  public:
    explicit CollisionSystem(const std::shared_ptr<fr::Scene>& scene) : System(scene) {}

    void PreUpdate(float deltaTime) override
    {
        mScene->CreateQuery()->EachAsync<Position>([&](Position& position) { mScene->SendEvent(CollisionEvent {}); });

        mScene->CreateQuery()->EachAsync<Position>([](Position& position) { position.x += 1; });
    }
};
