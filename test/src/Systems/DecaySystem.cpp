#include "DecaySystem.hpp"

#include "../Components/DecayComponent.hpp"

void DecaySystem::PreUpdate(float deltaTime, const Ref<fr::Scheduler>& scheduler)
{
    scheduler->Run<DecayComponent>("DecayUpdate", [this, deltaTime](auto entity, DecayComponent& component) {
        component.timeToLive -= deltaTime;

        if (component.timeToLive <= 0)
            mScene->DestroyEntity(entity);
    });
}