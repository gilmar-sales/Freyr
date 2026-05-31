#include "MovementSystem.hpp"

#include "../Components/DecayComponent.hpp"
#include "../Components/PositionComponent.hpp"

void MovementSystem::Update(float deltaTime)
{
    mRegistry->CreateMutation()->EachAsync<PositionComponent>([registry = mRegistry](auto entity, PositionComponent& position) {
        position.x += 1;

        registry->CreateEntity(position, DecayComponent { .timeToLive = 2.0f });
    });
}