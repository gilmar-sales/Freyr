#pragma once

#include "Freyr/Core/Pipeline.hpp"

#include "Freyr/Containers/SparseSet.hpp"

#include <algorithm>
#include <optional>

namespace FREYR_NAMESPACE
{
    class MutationAggregator;

    class SystemManager
    {
      public:
        explicit SystemManager(const skr::Arc<FreyrOptions>&       freyrOptions,
                               const skr::Arc<MutationAggregator>& mutationAggregator);

        ~SystemManager();

        int32_t RegisterPipeline(std::string_view name, float rate)
        {
            const int32_t id = static_cast<int32_t>(mPipelines.size());
            mPipelines.push_back(Pipeline(name, rate));
            return id;
        }

        [[nodiscard]] std::optional<int32_t> FindPipelineId(std::string_view name) const
        {
            for (int32_t i = 0; i < static_cast<int32_t>(mPipelines.size()); ++i)
            {
                if (mPipelines[static_cast<size_t>(i)].Name == name)
                    return i;
            }

            return std::nullopt;
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
            mSystemLabels.push_back(std::string(refl::type_name<T>()));
            mPipelines[static_cast<size_t>(pipelineId)].Systems.push_back(systemId);
        }

        template <typename T>
            requires IsSystem<T>
        [[nodiscard]] bool UnregisterSystem()
        {
            const auto systemId = GetSystemId<T>();

            if (!mSystems.contains(systemId))
                return false;

            for (auto& pipeline : mPipelines)
            {
                auto& systems = pipeline.Systems;
                systems.erase(std::remove(systems.begin(), systems.end(), systemId), systems.end());
            }

            const auto index = mSystems.getIndex(systemId);
            if (index != mSystemLabels.size() - 1)
                mSystemLabels[index] = std::move(mSystemLabels.back());
            mSystemLabels.pop_back();
            mSystems.remove(systemId);
            mSystemFactories[systemId] = {};

            return true;
        }

        template <typename T>
            requires IsSystem<T>
        [[nodiscard]] bool IsSystemRegistered() const
        {
            return mSystems.contains(GetSystemId<T>());
        }

        void Accumulate(float dt);

        void PreUpdate(float dt, const skr::Arc<skr::ServiceProvider>& serviceProvider);
        void Update(float dt, const skr::Arc<skr::ServiceProvider>& serviceProvider);
        void PostUpdate(float dt, const skr::Arc<skr::ServiceProvider>& serviceProvider);

        [[nodiscard]] std::string_view GetSystemLabel(const SystemId systemId) const
        {
            return mSystemLabels[mSystems.getIndex(systemId)];
        }

      private:
        enum class Phase
        {
            PreUpdate,
            Update,
            PostUpdate
        };

        void RunPhase(Phase phase, float dt, const skr::Arc<skr::ServiceProvider>& serviceProvider);

        [[nodiscard]] skr::Arc<System> GetSystem(const SystemId                   systemId,
                                            const skr::Arc<skr::ServiceProvider>& serviceProvider) const
        {
            return skr::ArcCast<System>(mSystemFactories[systemId](*serviceProvider));
        }

        skr::Arc<MutationAggregator> mMutationAggregator;

        SparseSet<SystemId>              mSystems;
        std::vector<skr::ServiceFactory> mSystemFactories;
        std::vector<std::string>         mSystemLabels;
        std::vector<Pipeline>            mPipelines;
        std::vector<Pipeline*>           mReadyPipelines;
    };

} // namespace FREYR_NAMESPACE
