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
        mScene->CreateQuery()
            ->Each<Position>([&](Position& position) { mScene->SendEvent(CollisionEvent {}); })
            .Schedule();

        mScene->CreateQuery()->Each<Position>([](Position& position) { position.x += 1; }).Schedule();
    }
};
