#pragma once

#include <Freyr.hpp>

#include "../components/position.hpp"

class CollisionSystem : public fr::System
{
    void Update(float deltaTime)
    {
        mManager->ForEachAsync<Position>("Send collisions", [&](Position& position) mutable
        {
            mManager->SendEvent(CollisionEvent{});
        });

        mManager->ForEachAsync<Position>("Update positions", [&](Position& position) mutable
        {
            position.x += 1;
        });
    }
};
