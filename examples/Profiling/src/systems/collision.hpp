#pragma once

#include <Freyr.hpp>

#include "../components/position.hpp"

class CollisionSystem : public fr::System
{
    void Update(float deltaTime)
    {
        mManager->ForEach<Position>("Send collisions", [&](fr::Entity entity, Position& position) mutable
        {
            mManager->SendEvent(CollisionEvent{});
        });

        mManager->ForEachAsync<Position>("Update positions", [&](fr::Entity entity, Position& position) mutable
        {
            position.x += 1;
        });
    }
};
