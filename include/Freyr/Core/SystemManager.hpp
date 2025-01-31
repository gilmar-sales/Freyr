#pragma once

#include <Skirnir.hpp>

#include "Freyr/Base/Entity.hpp"
#include "Freyr/Base/System.hpp"
#include "Freyr/Containers/SparseSet.hpp"

namespace FREYR_NAMESPACE
{

    class SystemManager
    {
      public:
        explicit SystemManager(const std::uint64_t initialCapacity)
        {
            mSystemFactories.resize(initialCapacity);

            mRegisteredSystems.resize(initialCapacity);
        };

        ~SystemManager() = default;

        template <typename T>
            requires IsSystem<T>
        void RegisterSystem()
        {
            assert(!mRegisteredSystems.contains(GetSystemId<T>()) &&
                   "Registering system more than once.");

            mSystemFactories[GetSystemId<T>()] = [](ServiceProvider& provider) {
                return provider.GetService<T>();
            };

            mRegisteredSystems.insert(GetSystemId<T>());
        }

        void PreUpdate(const float                             dt,
                       const std::shared_ptr<ServiceProvider>& serviceProvider)
        {
            for (auto const& id : mRegisteredSystems.getDense())
            {
                GetSystem(id, serviceProvider)->PreUpdate(dt);
            }
        }

        void Update(const float                             dt,
                    const std::shared_ptr<ServiceProvider>& serviceProvider)
        {
            for (auto const& id : mRegisteredSystems.getDense())
            {
                GetSystem(id, serviceProvider)->Update(dt);
            }
        }

        void PostUpdate(const float                             dt,
                        const std::shared_ptr<ServiceProvider>& serviceProvider)
        {
            for (auto const& id : mRegisteredSystems.getDense())
            {
                GetSystem(id, serviceProvider)->PostUpdate(dt);
            }
        }

      private:
        [[nodiscard]] std::shared_ptr<System> GetSystem(
            const unsigned long                     systemId,
            const std::shared_ptr<ServiceProvider>& serviceProvider) const
        {
            return std::static_pointer_cast<System>(
                mSystemFactories[systemId](*serviceProvider));
        }

        std::vector<ServiceFactory> mSystemFactories;
        SparseSet<SystemId>         mRegisteredSystems;
    };

} // namespace FREYR_NAMESPACE
