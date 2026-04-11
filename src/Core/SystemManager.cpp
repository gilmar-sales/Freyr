#include <Freyr/Core/SystemManager.hpp>

#include <Freyr/Core/Profiling.hpp>
#include <Freyr/Core/Scheduler.hpp>

namespace FREYR_NAMESPACE
{

    Ref<Scheduler> SystemManager::PreUpdate(const float deltaTime, const Ref<skr::ServiceProvider>& serviceProvider)
    {
        FREYR_TRACE("FREYR", "Schedule: PreUpdate");

        auto scheduler = serviceProvider->GetService<Scheduler>();

        for (auto const& id : mRegisteredSystems.getDense())
        {
            FREYR_TRACE("FREYR", GetSystemLabel(id).data());
            GetSystem(id, serviceProvider)->PreUpdate(deltaTime, scheduler);
        }

        return scheduler;
    }

    Ref<Scheduler> SystemManager::Update(const float deltaTime, const Ref<skr::ServiceProvider>& serviceProvider)
    {
        FREYR_TRACE("FREYR", "Schedule: Update");

        auto scheduler = serviceProvider->GetService<Scheduler>();

        for (auto const& id : mRegisteredSystems.getDense())
        {
            FREYR_TRACE("FREYR", GetSystemLabel(id).data());
            GetSystem(id, serviceProvider)->Update(deltaTime, scheduler);
        }

        return scheduler;
    }

    Ref<Scheduler> SystemManager::PostUpdate(const float deltaTime, const Ref<skr::ServiceProvider>& serviceProvider)
    {
        FREYR_TRACE("FREYR", "Schedule: PostUpdate");

        auto scheduler = serviceProvider->GetService<Scheduler>();

        for (auto const& id : mRegisteredSystems.getDense())
        {
            FREYR_TRACE("FREYR", GetSystemLabel(id).data());
            GetSystem(id, serviceProvider)->PostUpdate(deltaTime, scheduler);
        }

        return scheduler;
    }

    Ref<Scheduler> SystemManager::PreFixedUpdate(const float                      deltaTime,
                                                 const Ref<skr::ServiceProvider>& serviceProvider)
    {
        FREYR_TRACE("FREYR", "Schedule: PreFixedUpdate");

        auto scheduler = serviceProvider->GetService<Scheduler>();

        for (auto const& id : mRegisteredSystems.getDense())
        {
            FREYR_TRACE("FREYR", GetSystemLabel(id).data());
            GetSystem(id, serviceProvider)->PreFixedUpdate(deltaTime, scheduler);
        }

        return scheduler;
    }

    Ref<Scheduler> SystemManager::FixedUpdate(const float deltaTime, const Ref<skr::ServiceProvider>& serviceProvider)
    {
        FREYR_TRACE("FREYR", "Schedule: FixedUpdate");

        auto scheduler = serviceProvider->GetService<Scheduler>();

        for (auto const& id : mRegisteredSystems.getDense())
        {
            FREYR_TRACE("FREYR", GetSystemLabel(id).data());
            GetSystem(id, serviceProvider)->FixedUpdate(deltaTime, scheduler);
        }

        return scheduler;
    }

    Ref<Scheduler> SystemManager::PostFixedUpdate(const float                      deltaTime,
                                                  const Ref<skr::ServiceProvider>& serviceProvider)
    {
        FREYR_TRACE("FREYR", "Schedule: PostFixedUpdate");

        auto scheduler = serviceProvider->GetService<Scheduler>();

        for (auto const& id : mRegisteredSystems.getDense())
        {
            FREYR_TRACE("FREYR", GetSystemLabel(id).data());
            GetSystem(id, serviceProvider)->PostFixedUpdate(deltaTime, scheduler);
        }

        return scheduler;
    }
} // namespace FREYR_NAMESPACE