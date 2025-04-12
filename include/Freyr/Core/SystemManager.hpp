#pragma once

#include <Skirnir/Skirnir.hpp>

#include "Freyr/Base/Entity.hpp"
#include "Freyr/Base/System.hpp"
#include "Freyr/Containers/SparseSet.hpp"

namespace FREYR_NAMESPACE
{

    class SystemManager
    {
      public:
        explicit SystemManager(const Ref<FreyrOptions>& freyrOptions)
        {
            mSystemFactories.resize(freyrOptions->InitialCapacity);
            mRegisteredSystems.resize(freyrOptions->InitialCapacity);
        };

        ~SystemManager() = default;

        template <typename T>
            requires IsSystem<T>
        void RegisterSystem()
        {
            assert(!mRegisteredSystems.contains(GetSystemId<T>()) &&
                   "Registering system more than once.");

            mSystemFactories[GetSystemId<T>()] =
                [](skr::ServiceProvider& provider) {
                    return provider.GetService<T>();
                };

            mRegisteredSystems.insert(GetSystemId<T>());
        }

        void PreUpdate(const float                      dt,
                       const Ref<skr::ServiceProvider>& serviceProvider)
        {
            for (auto const& id : mRegisteredSystems.getDense())
            {
                GetSystem(id, serviceProvider)->PreUpdate(dt);
            }
        }

        void Update(const float                      dt,
                    const Ref<skr::ServiceProvider>& serviceProvider)
        {
            for (auto const& id : mRegisteredSystems.getDense())
            {
                GetSystem(id, serviceProvider)->Update(dt);
            }
        }

        void PostUpdate(const float                      dt,
                        const Ref<skr::ServiceProvider>& serviceProvider)
        {
            for (auto const& id : mRegisteredSystems.getDense())
            {
                GetSystem(id, serviceProvider)->PostUpdate(dt);
            }
        }

      private:
        [[nodiscard]] Ref<System> GetSystem(
            const unsigned long              systemId,
            const Ref<skr::ServiceProvider>& serviceProvider) const
        {
            return std::static_pointer_cast<System>(
                mSystemFactories[systemId](*serviceProvider));
        }

        std::vector<skr::ServiceFactory> mSystemFactories;
        SparseSet<SystemId>              mRegisteredSystems;
    };

} // namespace FREYR_NAMESPACE
