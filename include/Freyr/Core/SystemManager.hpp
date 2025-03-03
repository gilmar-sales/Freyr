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
        explicit SystemManager(
            const std::shared_ptr<FreyrOptions>& freyrOptions) :
            mRegisteredSystems(freyrOptions->MaxSystems)
        {
            mSystemFactories.resize(freyrOptions->MaxSystems);
        };

        ~SystemManager() = default;

        template <typename T>
            requires IsSystem<T>
        void RegisterSystem()
        {
            FREYR_ASSERT(!mRegisteredSystems.contains(GetSystemId<T>()) &&
                         "Registering system more than once.");

            mSystemFactories[GetSystemId<T>()] = [](ServiceProvider& provider) {
                return provider.GetService<T>();
            };

            mRegisteredSystems.insert(GetSystemId<T>());
            mSystemLabels.push_back(typeid(T).name());
        }

        void PreUpdate(const float                             dt,
                       const std::shared_ptr<ServiceProvider>& serviceProvider)
        {
            for (auto const& id : mRegisteredSystems.getDense())
            {
                FREYR_PROFILING_BEGIN("FREYR",
                                      GetSystemLabel(id).c_str(),
                                      perfetto::Track(1));
                GetSystem(id, serviceProvider)->PreUpdate(dt);
                FREYR_PROFILING_END("FREYR", perfetto::Track(1));
            }
        }

        void Update(const float                             dt,
                    const std::shared_ptr<ServiceProvider>& serviceProvider)
        {
            for (auto const& id : mRegisteredSystems.getDense())
            {
                FREYR_PROFILING_BEGIN("FREYR",
                                      GetSystemLabel(id).c_str(),
                                      perfetto::Track(1));
                GetSystem(id, serviceProvider)->Update(dt);
                FREYR_PROFILING_END("FREYR", perfetto::Track(1));
            }
        }

        void PostUpdate(const float                             dt,
                        const std::shared_ptr<ServiceProvider>& serviceProvider)
        {
            for (auto const& id : mRegisteredSystems.getDense())
            {
                FREYR_PROFILING_BEGIN("FREYR",
                                      GetSystemLabel(id).c_str(),
                                      perfetto::Track(1));
                GetSystem(id, serviceProvider)->PostUpdate(dt);
                FREYR_PROFILING_END("FREYR", perfetto::Track(1));
            }
        }

      private:
        [[nodiscard]] std::shared_ptr<System> GetSystem(
            const SystemId                          systemId,
            const std::shared_ptr<ServiceProvider>& serviceProvider) const
        {
            return std::static_pointer_cast<System>(
                mSystemFactories[mRegisteredSystems.getIndex(systemId)](
                    *serviceProvider));
        }

        const std::string& GetSystemLabel(const SystemId systemId) const
        {
            return mSystemLabels[mRegisteredSystems.getIndex(systemId)];
        }

        std::vector<ServiceFactory> mSystemFactories;
        std::vector<std::string>    mSystemLabels;
        SparseSet<SystemId>         mRegisteredSystems;
    };

} // namespace FREYR_NAMESPACE
