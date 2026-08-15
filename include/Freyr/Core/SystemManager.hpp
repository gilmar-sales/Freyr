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

        /// When disabled, the pipeline is skipped for Pre/Update/Post and its rate
        /// accumulator is cleared (no catch-up burst when re-enabled).
        void SetPipelineEnabled(int32_t pipelineId, bool enabled)
        {
            FREYR_ASSERT(pipelineId >= 0 && pipelineId < static_cast<int32_t>(mPipelines.size()) &&
                         "Invalid pipeline id.");
            auto& pipeline   = mPipelines[static_cast<size_t>(pipelineId)];
            pipeline.Enabled = enabled;
            if (!enabled)
                pipeline.Accumulator = 0.0f;
        }

        [[nodiscard]] bool IsPipelineEnabled(int32_t pipelineId) const
        {
            FREYR_ASSERT(pipelineId >= 0 && pipelineId < static_cast<int32_t>(mPipelines.size()) &&
                         "Invalid pipeline id.");
            return mPipelines[static_cast<size_t>(pipelineId)].Enabled;
        }

        template <typename T>
            requires IsSystem<T>
        void RegisterSystem(int32_t pipelineId)
        {
            RegisterSystem<T>(pipelineId, static_cast<std::size_t>(-1));
        }

        template <typename T>
            requires IsSystem<T>
        void RegisterSystem(int32_t pipelineId, std::size_t index)
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
            InsertSystemInPipeline(pipelineId, systemId, index);
        }

        template <typename T>
            requires IsSystem<T>
        [[nodiscard]] bool UnregisterSystem()
        {
            const auto systemId = GetSystemId<T>();

            if (!mSystems.contains(systemId))
                return false;

            RemoveSystemFromPipelines(systemId);

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

        [[nodiscard]] int32_t GetPipelineCount() const
        {
            return static_cast<int32_t>(mPipelines.size());
        }

        [[nodiscard]] PipelineView GetPipeline(const int32_t pipelineId) const
        {
            FREYR_ASSERT(pipelineId >= 0 && pipelineId < static_cast<int32_t>(mPipelines.size()) &&
                         "Invalid pipeline id.");
            const auto& pipeline = mPipelines[static_cast<size_t>(pipelineId)];
            return PipelineView { .Id      = pipelineId,
                                  .Name    = pipeline.Name,
                                  .Rate    = pipeline.Rate,
                                  .Enabled = pipeline.Enabled,
                                  .Systems = std::span<const SystemId>(pipeline.Systems) };
        }

        void ForEachPipeline(auto&& function) const
        {
            for (int32_t i = 0; i < GetPipelineCount(); ++i)
            {
                function(GetPipeline(i));
            }
        }

        void ForEachRegisteredSystem(auto&& function) const
        {
            for (const auto systemId : mSystems)
            {
                function(systemId, GetSystemLabel(systemId));
            }
        }

        [[nodiscard]] std::optional<int32_t> FindPipelineContaining(const SystemId systemId) const
        {
            for (int32_t i = 0; i < GetPipelineCount(); ++i)
            {
                const auto& systems = mPipelines[static_cast<size_t>(i)].Systems;
                if (std::find(systems.begin(), systems.end(), systemId) != systems.end())
                    return i;
            }

            return std::nullopt;
        }

        void SetPipelineName(const int32_t pipelineId, const std::string_view name)
        {
            FREYR_ASSERT(pipelineId >= 0 && pipelineId < static_cast<int32_t>(mPipelines.size()) &&
                         "Invalid pipeline id.");
            mPipelines[static_cast<size_t>(pipelineId)].Name = std::string(name);
        }

        void SetPipelineRate(const int32_t pipelineId, const float rate)
        {
            FREYR_ASSERT(pipelineId >= 0 && pipelineId < static_cast<int32_t>(mPipelines.size()) &&
                         "Invalid pipeline id.");
            mPipelines[static_cast<size_t>(pipelineId)].Rate = rate > 0.0f ? 1.0f / rate : 0.0f;
        }

        [[nodiscard]] bool MoveSystem(const SystemId    systemId,
                                      const int32_t     pipelineId,
                                      const std::size_t index)
        {
            if (!mSystems.contains(systemId))
                return false;

            FREYR_ASSERT(pipelineId >= 0 && pipelineId < static_cast<int32_t>(mPipelines.size()) &&
                         "Invalid pipeline id.");

            RemoveSystemFromPipelines(systemId);
            InsertSystemInPipeline(pipelineId, systemId, index);
            return true;
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

        void InsertSystemInPipeline(const int32_t pipelineId, const SystemId systemId,
                                    const std::size_t index)
        {
            auto& systems = mPipelines[static_cast<size_t>(pipelineId)].Systems;
            if (index >= systems.size())
                systems.push_back(systemId);
            else
                systems.insert(systems.begin() + static_cast<std::ptrdiff_t>(index), systemId);
        }

        void RemoveSystemFromPipelines(const SystemId systemId)
        {
            for (auto& pipeline : mPipelines)
            {
                auto& systems = pipeline.Systems;
                systems.erase(std::remove(systems.begin(), systems.end(), systemId), systems.end());
            }
        }

        skr::Arc<MutationAggregator> mMutationAggregator;

        SparseSet<SystemId>              mSystems;
        std::vector<skr::ServiceFactory> mSystemFactories;
        std::vector<std::string>         mSystemLabels;
        std::vector<Pipeline>            mPipelines;
        std::vector<Pipeline*>           mReadyPipelines;
    };

} // namespace FREYR_NAMESPACE
