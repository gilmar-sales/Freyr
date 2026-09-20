#include <Freyr/Core/SystemManager.hpp>

#include <Freyr/Core/MutationAggregator.hpp>
#include <Freyr/Core/Profiling.hpp>
#include <Freyr/Core/Registry.hpp>

#include <algorithm>
#include <ranges>

namespace FREYR_NAMESPACE
{
    SystemManager::SystemManager(const skr::Arc<FreyrOptions>&       freyrOptions,
                                 const skr::Arc<MutationAggregator>& mutationAggregator) :
        mSystems(freyrOptions->MaxSystems), mMutationAggregator(mutationAggregator)
    {
        mSystemFactories.resize(freyrOptions->MaxSystems);
        mSystemDetachers.resize(freyrOptions->MaxSystems);
    }

    SystemManager::~SystemManager() = default;

    void SystemManager::SortPipelineSystems(const int32_t pipelineId)
    {
        auto& systems = PipelineAt(pipelineId).Systems;
        if (systems.size() <= 1)
            return;

        std::unordered_map<SystemId, std::vector<SystemId>> edges;
        std::unordered_map<SystemId, int>                   indegree;
        std::unordered_map<SystemId, std::size_t>           originalOrder;

        for (std::size_t i = 0; i < systems.size(); ++i)
        {
            const SystemId id = systems[i];
            edges[id]         = {};
            indegree[id]      = 0;
            originalOrder[id] = i;
        }

        for (const SystemId id : systems)
        {
            const auto it = mScheduleMeta.find(id);
            if (it == mScheduleMeta.end())
                continue;

            for (const SystemId afterId : it->second.after)
            {
                if (!indegree.contains(afterId))
                    continue;
                edges[afterId].push_back(id);
                ++indegree[id];
            }

            for (const SystemId beforeId : it->second.before)
            {
                if (!indegree.contains(beforeId))
                    continue;
                edges[id].push_back(beforeId);
                ++indegree[beforeId];
            }
        }

        std::vector<SystemId> ready;
        for (const SystemId id : systems)
        {
            if (indegree[id] == 0)
                ready.push_back(id);
        }

        std::vector<SystemId> sorted;
        sorted.reserve(systems.size());
        while (!ready.empty())
        {
            const auto bestIt =
                std::ranges::min_element(ready,
                                         [&](SystemId a, SystemId b)
                                         { return originalOrder[a] < originalOrder[b]; });
            const SystemId best = *bestIt;
            ready.erase(bestIt);
            sorted.push_back(best);

            for (const SystemId next : edges[best])
            {
                if (--indegree[next] == 0)
                    ready.push_back(next);
            }
        }

        FREYR_ASSERT(sorted.size() == systems.size() && "Cycle detected in system After/Before");
        systems = std::move(sorted);
    }

    void SystemManager::Accumulate(float dt)
    {
        mReadyPipelineIds.clear();

        if (mPipelinesDirty)
        {
            for (const auto& pipeline : mPipelines)
                SortPipelineSystems(pipeline.Id);
            mPipelinesDirty = false;
        }

        for (auto& pipeline : mPipelines)
        {
            if (!pipeline.Enabled)
            {
                pipeline.Accumulator = 0.0f;
                continue;
            }

            if (pipeline.Rate <= 0.0f)
            {
                mReadyPipelineIds.push_back(pipeline.Id);
                continue;
            }

            pipeline.Accumulator += dt;

            if (pipeline.Accumulator >= pipeline.Rate)
            {
                mReadyPipelineIds.push_back(pipeline.Id);

                pipeline.Accumulator -= pipeline.Rate;
            }
        }
    }

    void SystemManager::RunPhase(const Phase                           phase,
                                 const float                           dt,
                                 const skr::Arc<skr::ServiceProvider>& serviceProvider)
    {
        const char* scheduleLabel =
            phase == Phase::PreUpdate ? "Schedule: PreUpdate"
            : phase == Phase::Update  ? "Schedule: Update"
                                      : "Schedule: PostUpdate";

        FREYR_TRACE_BEGIN("FREYR", scheduleLabel);

        auto registry = serviceProvider->GetService<Registry>();

        for (const auto pipelineId : mReadyPipelineIds)
        {
            if (!HasPipeline(pipelineId))
                continue;

            const auto& pipeline = GetPipeline(pipelineId);

            FREYR_TRACE_BEGIN("FREYR", pipeline.Name.data());

            const float effectiveDt = pipeline.Rate == 0.0f ? dt : pipeline.Rate;

            const auto systems =
                std::vector<SystemId>(pipeline.Systems.begin(), pipeline.Systems.end());

            for (const auto id : systems)
            {
                if (!IsSystemRegistered(id))
                    continue;

                if (const auto meta = mScheduleMeta.find(id); meta != mScheduleMeta.end())
                {
                    if (meta->second.runIf && registry && !meta->second.runIf(*registry))
                        continue;
                }

                FREYR_TRACE_BEGIN("FREYR", GetSystemLabel(id).data());
                auto* system = GetSystem(id, serviceProvider).get();

                switch (phase)
                {
                    case Phase::PreUpdate:
                        system->PreUpdate(effectiveDt);
                        break;
                    case Phase::Update:
                        system->Update(effectiveDt);
                        break;
                    case Phase::PostUpdate:
                        system->PostUpdate(effectiveDt);
                        break;
                }

                FREYR_TRACE_END("FREYR");
            }

            FREYR_TRACE_END("FREYR");
        }

        mMutationAggregator->Flush();
        FREYR_TRACE_END("FREYR");
    }

    void SystemManager::PreUpdate(const float                           dt,
                                  const skr::Arc<skr::ServiceProvider>& serviceProvider)
    {
        RunPhase(Phase::PreUpdate, dt, serviceProvider);
    }

    void SystemManager::Update(const float                           dt,
                               const skr::Arc<skr::ServiceProvider>& serviceProvider)
    {
        RunPhase(Phase::Update, dt, serviceProvider);
    }

    void SystemManager::PostUpdate(const float                           dt,
                                   const skr::Arc<skr::ServiceProvider>& serviceProvider)
    {
        RunPhase(Phase::PostUpdate, dt, serviceProvider);
    }

} // namespace FREYR_NAMESPACE
