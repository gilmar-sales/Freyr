#include "MovementSystem.hpp"

#include "../Components/DecayComponent.hpp"
#include "../Components/PositionComponent.hpp"
#include "DecaySystem.hpp"

void MovementSystem::FixedUpdate(float deltaTime, const Ref<fr::Scheduler>& scheduler)
{
    scheduler->Run<PositionComponent>("MovementUpdate", [this](auto entity, PositionComponent& position) {
        position.x += 1;

        mScene->CreateEntity(
            [](auto entity, PositionComponent& position, DecayComponent& decay) {
                auto x = position.x;
                auto y = position.y;
            },
            position, DecayComponent { .timeToLive = 2.0f });
    });
}