#pragma once

#include "Freyr/Core/Pipeline.hpp"

#include "Freyr/Containers/SparseSet.hpp"

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

        int32_t RegisterPipeline(std::string_view name, float rate)
        {
            const int32_t id = static_cast<int32_t>(mPipelines.size());
            mPipelines.push_back(Pipeline(name, rate));
            return id;
        }

        template <typename T>
            requires IsSystem<T>
        void RegisterSystem(int32_t pipelineId)
        {
            FREYR_ASSERT(!mRegisteredSystems.contains(GetSystemId<T>()) && "Registering system more than once.");
            FREYR_ASSERT(pipelineId >= 0 && pipelineId < static_cast<int32_t>(mPipelines.size()) && "Invalid pipeline id.");

            mSystemFactories[GetSystemId<T>()] = [](skr::ServiceProvider& provider) {
                return provider.GetService<T>();
            };

            mRegisteredSystems.insert(GetSystemId<T>());
            mSystemLabels.push_back(skr::type_name<T>());
            mPipelines[pipelineId].Systems.push_back(GetSystemId<T>());
        }

        void Update(float dt, const Ref<skr::ServiceProvider>& serviceProvider);

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

        void UpdatePipeline(Pipeline& pipeline, float dt, const Ref<skr::ServiceProvider>& serviceProvider);

        std::vector<skr::ServiceFactory> mSystemFactories;
        std::vector<std::string_view>    mSystemLabels;
        SparseSet<SystemId>              mRegisteredSystems;
        std::vector<Pipeline>           mPipelines;
    };

} // namespace FREYR_NAMESPACE
