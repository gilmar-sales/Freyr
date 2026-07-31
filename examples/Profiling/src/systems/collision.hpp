#pragma once

#include <Freyr/Freyr.hpp>

#include "../components/position.hpp"
#include "../events/collision.hpp"

class CollisionSystem final : public fr::System
{
  public:
    explicit CollisionSystem(const Ref<fr::Registry>& registry) : System(registry) {}

    void PreUpdate(float deltaTime) override
    {
        mRegistry->CreateMutation()
            ->WithLabel("Send collisions")
            .EachAsync<Position>([&](Position& position) {
                mRegistry->SendEvent(CollisionEvent {});
            });

        mRegistry->CreateMutation()
            ->WithLabel("Update more positions")
            .EachAsync<Position>([](Position& position) { position.x += 1; });
    }
};
