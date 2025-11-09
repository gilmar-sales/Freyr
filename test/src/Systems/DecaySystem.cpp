#include "DecaySystem.hpp"

#include "../Components/DecayComponent.hpp"

void DecaySystem::PreUpdate(float deltaTime)
{
    mScene->ForEachAsync<DecayComponent>(
        [deltaTime, scene = mScene](auto entity, DecayComponent& component) {
            component.timeToLive -= deltaTime;

            if (component.timeToLive <= 0)
                scene->DestroyEntity(entity);
        });
}