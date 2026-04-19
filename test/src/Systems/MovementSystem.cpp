#include "MovementSystem.hpp"

#include "../Components/DecayComponent.hpp"
#include "../Components/PositionComponent.hpp"

void MovementSystem::Update(float deltaTime)
{
    mScene->ForEachAsync<PositionComponent>([scene = mScene](auto entity, PositionComponent& position) {
        position.x += 1;

        scene->CreateEntity(
            [](auto entity, PositionComponent& position, DecayComponent& decay) {
                auto x = position.x;
                auto y = position.y;
            },
            position, DecayComponent { .timeToLive = 2.0f });
    });
}