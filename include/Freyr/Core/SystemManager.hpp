#pragma once

#include "Freyr/Core/Pipeline.hpp"
#include "MutationAggregator.hpp"

#include "Freyr/Containers/SparseSet.hpp"

namespace FREYR_NAMESPACE
{

    class SystemManager
    {
      public:
        explicit SystemManager(const skr::Arc<FreyrOptions>&       freyrOptions,
                               const skr::Arc<MutationAggregator>& mutationAggregator) :
            mSystems(freyrOptions->MaxSystems), mMutationAggregator(mutationAggregator)
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
            const auto systemId = GetSystemId<T>();

            FREYR_ASSERT(!mSystems.contains(systemId) && "Registering system more than once.");
            FREYR_ASSERT(pipelineId >= 0 && pipelineId < static_cast<int32_t>(mPipelines.size()) &&
                         "Invalid pipeline id.");
            FREYR_ASSERT(systemId < mSystemFactories.size() && "System id exceeds MaxSystems.");

            mSystemFactories[systemId] = [](skr::ServiceProvider& provider) {
                return provider.GetService<T>();
            };

            mSystems.insert(systemId);
            mSystemLabels.push_back(refl::type_name<T>());
            mPipelines[pipelineId].Systems.push_back(systemId);
        }

        void Accumulate(float dt);

        void PreUpdate(float dt, const skr::Arc<skr::ServiceProvider>& serviceProvider);
        void Update(float dt, const skr::Arc<skr::ServiceProvider>& serviceProvider);
        void PostUpdate(float dt, const skr::Arc<skr::ServiceProvider>& serviceProvider);

      private:
        [[nodiscard]] skr::Arc<System> GetSystem(const SystemId                   systemId,
                                            const skr::Arc<skr::ServiceProvider>& serviceProvider) const
        {
            return skr::ArcCast<System>(mSystemFactories[systemId](*serviceProvider));
        }

        std::string_view GetSystemLabel(const SystemId systemId) const
        {
            return mSystemLabels[mSystems.getIndex(systemId)];
        }

        skr::Arc<MutationAggregator> mMutationAggregator;

        SparseSet<SystemId>              mSystems;
        std::vector<skr::ServiceFactory> mSystemFactories;
        std::vector<std::string_view>    mSystemLabels;
        std::vector<Pipeline>            mPipelines;
        std::vector<Pipeline*>           mReadyPipelines;
    };

} // namespace FREYR_NAMESPACE
