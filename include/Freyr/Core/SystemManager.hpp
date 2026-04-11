#pragma once

#include "Freyr/Base/System.hpp"
#include "Freyr/Containers/SparseSet.hpp"
#include "Freyr/Core/Scheduler.hpp"

namespace FREYR_NAMESPACE
{

    class SystemManager
    {
      public:
        explicit SystemManager(const Ref<FreyrOptions>& freyrOptions) : mRegisteredSystems(freyrOptions->MaxSystems)
        {
            mSystemFactories.resize(freyrOptions->MaxSystems);
        };

        ~SystemManager() = default;

        template <typename T>
            requires IsSystem<T>
        void RegisterSystem()
        {
            FREYR_ASSERT(!mRegisteredSystems.contains(GetSystemId<T>()) && "Registering system more than once.");

            mSystemFactories[GetSystemId<T>()] = [](skr::ServiceProvider& provider) {
                return provider.GetService<T>();
            };

            mRegisteredSystems.insert(GetSystemId<T>());
            mSystemLabels.push_back(skr::type_name<T>());
        }

        Ref<Scheduler> PreUpdate(float deltaTime, const Ref<skr::ServiceProvider>& serviceProvider);

        Ref<Scheduler> Update(float deltaTime, const Ref<skr::ServiceProvider>& serviceProvider);

        Ref<Scheduler> PostUpdate(float deltaTime, const Ref<skr::ServiceProvider>& serviceProvider);

        Ref<Scheduler> PreFixedUpdate(float deltaTime, const Ref<skr::ServiceProvider>& serviceProvider);

        Ref<Scheduler> FixedUpdate(float deltaTime, const Ref<skr::ServiceProvider>& serviceProvider);

        Ref<Scheduler> PostFixedUpdate(float deltaTime, const Ref<skr::ServiceProvider>& serviceProvider);

      private:
        [[nodiscard]] Ref<System> GetSystem(const SystemId                   systemId,
                                            const Ref<skr::ServiceProvider>& serviceProvider) const
        {
            return std::static_pointer_cast<System>(
                mSystemFactories[mRegisteredSystems.getIndex(systemId)](*serviceProvider));
        }

        std::string_view GetSystemLabel(const SystemId systemId) const
        {
            return mSystemLabels[mRegisteredSystems.getIndex(systemId)];
        }

        std::vector<skr::ServiceFactory> mSystemFactories;
        std::vector<std::string_view>    mSystemLabels;
        SparseSet<SystemId>              mRegisteredSystems;
    };

} // namespace FREYR_NAMESPACE