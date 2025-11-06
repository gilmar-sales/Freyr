#include "MovementSystem.hpp"

#include "../Components/DecayComponent.hpp"
#include "../Components/PositionComponent.hpp"
#include "DecaySystem.hpp"

void MovementSystem::FixedUpdate(float deltaTime)
{
    mScene->ForEachAsync<PositionComponent>(
        [scene = mScene](auto entity, PositionComponent& component) {
            component.x += 1;

            scene->CreateEntity(component,
                                DecayComponent { .timeToLive = 2.0f });
        });
}