#include <Freyr/Core/SystemManager.hpp>

#include <Freyr/Core/Profiling.hpp>

namespace FREYR_NAMESPACE
{

    void SystemManager::PreUpdate(const float dt, const Ref<skr::ServiceProvider>& serviceProvider)
    {
        FREYR_TRACE("FREYR", "Schedule: PreUpdate");

        for (auto const& id : mRegisteredSystems.getDense())
        {
            FREYR_TRACE("FREYR", GetSystemLabel(id).data());
            GetSystem(id, serviceProvider)->PreUpdate(dt);
        }
    }

    void SystemManager::Update(const float dt, const Ref<skr::ServiceProvider>& serviceProvider)
    {
        FREYR_TRACE("FREYR", "Schedule: Update");

        for (auto const& id : mRegisteredSystems.getDense())
        {
            FREYR_TRACE("FREYR", GetSystemLabel(id).data());
            GetSystem(id, serviceProvider)->Update(dt);
        }
    }

    void SystemManager::PostUpdate(const float dt, const Ref<skr::ServiceProvider>& serviceProvider)
    {
        FREYR_TRACE("FREYR", "Schedule: PostUpdate");

        for (auto const& id : mRegisteredSystems.getDense())
        {
            FREYR_TRACE("FREYR", GetSystemLabel(id).data());
            GetSystem(id, serviceProvider)->PostUpdate(dt);
        }
    }

    void SystemManager::PreFixedUpdate(const float dt, const Ref<skr::ServiceProvider>& serviceProvider)
    {
        FREYR_TRACE("FREYR", "Schedule: PreFixedUpdate");

        for (auto const& id : mRegisteredSystems.getDense())
        {
            FREYR_TRACE("FREYR", GetSystemLabel(id).data());
            GetSystem(id, serviceProvider)->PreFixedUpdate(dt);
        }
    }

    void SystemManager::FixedUpdate(const float dt, const Ref<skr::ServiceProvider>& serviceProvider)
    {
        FREYR_TRACE("FREYR", "Schedule: FixedUpdate");

        for (auto const& id : mRegisteredSystems.getDense())
        {
            FREYR_TRACE("FREYR", GetSystemLabel(id).data());
            GetSystem(id, serviceProvider)->FixedUpdate(dt);
        }
    }

    void SystemManager::PostFixedUpdate(const float dt, const Ref<skr::ServiceProvider>& serviceProvider)
    {
        FREYR_TRACE("FREYR", "Schedule: PostFixedUpdate");

        for (auto const& id : mRegisteredSystems.getDense())
        {
            FREYR_TRACE("FREYR", GetSystemLabel(id).data());
            GetSystem(id, serviceProvider)->PostFixedUpdate(dt);
        }
    }
} // namespace FREYR_NAMESPACE
