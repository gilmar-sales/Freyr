#pragma once

#include "Freyr/Core/Pipeline.hpp"

#include "Freyr/Containers/SparseSet.hpp"

#include <algorithm>
#include <functional>
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
            return RegisterPipeline(name, rate, static_cast<std::size_t>(-1));
        }

        int32_t RegisterPipeline(std::string_view name, float rate, std::size_t index)
        {
            const int32_t id = mNextPipelineId++;
            Pipeline      pipeline(name, rate, id);

            if (index >= mPipelines.size())
                mPipelines.push_back(std::move(pipeline));
            else
                mPipelines.insert(mPipelines.begin() + static_cast<std::ptrdiff_t>(index),
                                  std::move(pipeline));

            return id;
        }

        [[nodiscard]] std::optional<int32_t> FindPipelineId(std::string_view name) const
        {
            for (const auto& pipeline : mPipelines)
            {
                if (pipeline.Name == name)
                    return pipeline.Id;
            }

            return std::nullopt;
        }

        [[nodiscard]] bool HasPipeline(const int32_t pipelineId) const
        {
            return FindPipelineIndex(pipelineId).has_value();
        }

        /// When disabled, the pipeline is skipped for Pre/Update/Post and its rate
        /// accumulator is cleared (no catch-up burst when re-enabled).
        void SetPipelineEnabled(int32_t pipelineId, bool enabled)
        {
            auto& pipeline   = PipelineAt(pipelineId);
            pipeline.Enabled = enabled;
            if (!enabled)
                pipeline.Accumulator = 0.0f;
        }

        [[nodiscard]] bool IsPipelineEnabled(int32_t pipelineId) const
        {
            return PipelineAt(pipelineId).Enabled;
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
            FREYR_ASSERT(HasPipeline(pipelineId) && "Invalid pipeline id.");
            FREYR_ASSERT(systemId < mSystemFactories.size() && "System id exceeds MaxSystems.");

            mSystemFactories[systemId] = [](skr::ServiceProvider& provider) {
                return provider.GetService<T>();
            };
            mSystemDetachers[systemId] = [](skr::ServiceProvider& provider) {
                provider.Remove<T>();
            };

            mSystems.insert(systemId);
            mSystemLabels.push_back(std::string(refl::type_name<T>()));
            InsertSystemInPipeline(pipelineId, systemId, index);
        }

        template <typename T>
            requires IsSystem<T>
        [[nodiscard]] bool UnregisterSystem()
        {
            return UnregisterSystem(GetSystemId<T>());
        }

        [[nodiscard]] bool UnregisterSystem(const SystemId systemId)
        {
            if (!mSystems.contains(systemId))
                return false;

            RemoveSystemFromPipelines(systemId);

            const auto index = mSystems.getIndex(systemId);
            if (index != mSystemLabels.size() - 1)
                mSystemLabels[index] = std::move(mSystemLabels.back());
            mSystemLabels.pop_back();
            mSystems.remove(systemId);
            mSystemFactories[systemId] = {};
            mSystemDetachers[systemId] = {};

            return true;
        }

        void DetachSystemService(const SystemId systemId, skr::ServiceProvider& serviceProvider)
        {
            if (systemId >= mSystemDetachers.size() || !mSystemDetachers[systemId])
                return;

            mSystemDetachers[systemId](serviceProvider);
            mSystemDetachers[systemId] = {};
        }

        template <typename T>
            requires IsSystem<T>
        [[nodiscard]] bool IsSystemRegistered() const
        {
            return IsSystemRegistered(GetSystemId<T>());
        }

        [[nodiscard]] bool IsSystemRegistered(const SystemId systemId) const
        {
            return mSystems.contains(systemId);
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
            const auto& pipeline = PipelineAt(pipelineId);
            return MakePipelineView(pipeline);
        }

        void ForEachPipeline(auto&& function) const
        {
            for (const auto& pipeline : mPipelines)
            {
                function(MakePipelineView(pipeline));
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
            for (const auto& pipeline : mPipelines)
            {
                if (std::find(pipeline.Systems.begin(), pipeline.Systems.end(), systemId) !=
                    pipeline.Systems.end())
                    return pipeline.Id;
            }

            return std::nullopt;
        }

        void SetPipelineName(const int32_t pipelineId, const std::string_view name)
        {
            PipelineAt(pipelineId).Name = std::string(name);
        }

        void SetPipelineRate(const int32_t pipelineId, const float rate)
        {
            PipelineAt(pipelineId).Rate = rate > 0.0f ? 1.0f / rate : 0.0f;
        }

        [[nodiscard]] bool MoveSystem(const SystemId    systemId,
                                      const int32_t     pipelineId,
                                      const std::size_t index)
        {
            if (!mSystems.contains(systemId) || !HasPipeline(pipelineId))
                return false;

            RemoveSystemFromPipelines(systemId);
            InsertSystemInPipeline(pipelineId, systemId, index);
            return true;
        }

        [[nodiscard]] bool MovePipeline(const int32_t pipelineId, const std::size_t index)
        {
            const auto from = FindPipelineIndex(pipelineId);
            if (!from)
                return false;

            Pipeline pipeline = std::move(mPipelines[*from]);
            mPipelines.erase(mPipelines.begin() + static_cast<std::ptrdiff_t>(*from));

            if (index >= mPipelines.size())
                mPipelines.push_back(std::move(pipeline));
            else
                mPipelines.insert(mPipelines.begin() + static_cast<std::ptrdiff_t>(index),
                                  std::move(pipeline));

            return true;
        }

        [[nodiscard]] bool UnregisterPipeline(const int32_t pipelineId)
        {
            const auto index = FindPipelineIndex(pipelineId);
            if (!index)
                return false;

            const auto systems = mPipelines[*index].Systems;
            for (const auto systemId : systems)
                (void) UnregisterSystem(systemId);

            const auto current = FindPipelineIndex(pipelineId);
            if (!current)
                return false;

            mPipelines.erase(mPipelines.begin() + static_cast<std::ptrdiff_t>(*current));
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

        [[nodiscard]] skr::Arc<System> GetSystem(
            const SystemId systemId, const skr::Arc<skr::ServiceProvider>& serviceProvider) const
        {
            return skr::ArcCast<System>(mSystemFactories[systemId](*serviceProvider));
        }

        [[nodiscard]] static PipelineView MakePipelineView(const Pipeline& pipeline)
        {
            return PipelineView { .Id      = pipeline.Id,
                                  .Name    = pipeline.Name,
                                  .Rate    = pipeline.Rate,
                                  .Enabled = pipeline.Enabled,
                                  .Systems = std::span<const SystemId>(pipeline.Systems) };
        }

        [[nodiscard]] std::optional<std::size_t> FindPipelineIndex(const int32_t pipelineId) const
        {
            for (std::size_t i = 0; i < mPipelines.size(); ++i)
            {
                if (mPipelines[i].Id == pipelineId)
                    return i;
            }

            return std::nullopt;
        }

        [[nodiscard]] Pipeline& PipelineAt(const int32_t pipelineId)
        {
            const auto index = FindPipelineIndex(pipelineId);
            FREYR_ASSERT(index.has_value() && "Invalid pipeline id.");
            return mPipelines[*index];
        }

        [[nodiscard]] const Pipeline& PipelineAt(const int32_t pipelineId) const
        {
            const auto index = FindPipelineIndex(pipelineId);
            FREYR_ASSERT(index.has_value() && "Invalid pipeline id.");
            return mPipelines[*index];
        }

        void InsertSystemInPipeline(const int32_t pipelineId, const SystemId systemId,
                                    const std::size_t index)
        {
            auto& systems = PipelineAt(pipelineId).Systems;
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

        SparseSet<SystemId>                                     mSystems;
        std::vector<skr::ServiceFactory>                        mSystemFactories;
        std::vector<std::function<void(skr::ServiceProvider&)>> mSystemDetachers;
        std::vector<std::string>                                mSystemLabels;
        std::vector<Pipeline>                                   mPipelines;
        std::vector<int32_t>                                    mReadyPipelineIds;
        int32_t                                                 mNextPipelineId = 0;
    };

} // namespace FREYR_NAMESPACE
