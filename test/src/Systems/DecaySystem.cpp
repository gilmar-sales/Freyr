#include "DecaySystem.hpp"

#include <Freyr/Core/Registry.hpp>

#include "../Components/DecayComponent.hpp"

void DecaySystem::PreUpdate(float deltaTime)
{
    mRegistry->CreateMutation()->EachAsync<DecayComponent>(
        [deltaTime, registry = mRegistry](auto entity, DecayComponent& component) {
            component.timeToLive -= deltaTime;

            if (component.timeToLive <= 0)
                registry->DestroyEntity(entity);
        });
}