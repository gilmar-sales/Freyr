#include "DecaySystem.hpp"

#include "../Components/DecayComponent.hpp"

void DecaySystem::PreUpdate(float deltaTime)
{
    mRegistry->CreateQuery()->EachAsync<DecayComponent>(
        [deltaTime, registry = mRegistry](auto entity, DecayComponent& component) {
            component.timeToLive -= deltaTime;

            if (component.timeToLive <= 0)
                registry->DestroyEntity(entity);
        });
}