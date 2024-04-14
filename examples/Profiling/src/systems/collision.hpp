#pragma once

#include <Freyr.hpp>

#include "../components/position.hpp"

class CollisionSystem : public fr::System
{
    void Update(float deltaTime)
    {
        mManager->ForEach<Position>("Send collisions", [&](Position& position) mutable
        {
            mManager->SendEvent(CollisionEvent{});
        });

        std::print("{}", (std::uint64_t)mManager.get());

        mManager->ForEachAsync<Position>("Update positions", [&](Position& position) mutable
        {
            position.x += 1;
        });
    }
};
