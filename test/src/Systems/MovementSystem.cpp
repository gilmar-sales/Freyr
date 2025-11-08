#include "MovementSystem.hpp"

#include "../Components/DecayComponent.hpp"
#include "../Components/PositionComponent.hpp"
#include "DecaySystem.hpp"

void MovementSystem::FixedUpdate(float deltaTime)
{
    mScene->ForEachAsync<PositionComponent>([scene = mScene](auto entity, PositionComponent& position) {
        position.x += 1;

        scene->CreateEntity(position, DecayComponent { .timeToLive = 2.0f });
    });
}