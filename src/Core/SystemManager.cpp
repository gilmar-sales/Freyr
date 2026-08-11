#include <Freyr/Core/SystemManager.hpp>

#include <Freyr/Core/MutationAggregator.hpp>
#include <Freyr/Core/Profiling.hpp>

namespace FREYR_NAMESPACE
{
    SystemManager::SystemManager(const skr::Arc<FreyrOptions>&       freyrOptions,
                                 const skr::Arc<MutationAggregator>& mutationAggregator) :
        mSystems(freyrOptions->MaxSystems), mMutationAggregator(mutationAggregator)
    {
        mSystemFactories.resize(freyrOptions->MaxSystems);
    }

    SystemManager::~SystemManager() = default;

    void SystemManager::Accumulate(float dt)
    {
        mReadyPipelines.clear();

        for (auto& pipeline : mPipelines)
        {
            if (!pipeline.Enabled)
            {
                pipeline.Accumulator = 0.0f;
                continue;
            }

            if (pipeline.Rate <= 0.0f)
            {
                mReadyPipelines.push_back(&pipeline);
                continue;
            }

            pipeline.Accumulator += dt;

            if (pipeline.Accumulator >= pipeline.Rate)
            {
                mReadyPipelines.push_back(&pipeline);

                pipeline.Accumulator -= pipeline.Rate;
            }
        }
    }

    void SystemManager::RunPhase(const Phase                             phase,
                                 const float                             dt,
                                 const skr::Arc<skr::ServiceProvider>& serviceProvider)
    {
        const char* scheduleLabel = phase == Phase::PreUpdate   ? "Schedule: PreUpdate"
                                    : phase == Phase::Update    ? "Schedule: Update"
                                                                : "Schedule: PostUpdate";

        FREYR_TRACE_BEGIN("FREYR", scheduleLabel,
                          perfetto::Track(0, perfetto::ProcessTrack::Current()));

        for (auto pipeline : mReadyPipelines)
        {
            FREYR_TRACE_BEGIN("FREYR", pipeline->Name.data(),
                              perfetto::Track(0, perfetto::ProcessTrack::Current()));

            const float effectiveDt = pipeline->Rate == 0.0f ? dt : pipeline->Rate;

            for (const auto id : pipeline->Systems)
            {
                FREYR_TRACE_BEGIN("FREYR", GetSystemLabel(id).data(),
                                  perfetto::Track(0, perfetto::ProcessTrack::Current()));
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

                FREYR_TRACE_END("FREYR", perfetto::Track(0));
            }

            FREYR_TRACE_END("FREYR", perfetto::Track(0));
        }

        mMutationAggregator->Flush();
        FREYR_TRACE_END("FREYR", perfetto::Track(0));
    }

    void SystemManager::PreUpdate(const float dt, const skr::Arc<skr::ServiceProvider>& serviceProvider)
    {
        RunPhase(Phase::PreUpdate, dt, serviceProvider);
    }

    void SystemManager::Update(const float dt, const skr::Arc<skr::ServiceProvider>& serviceProvider)
    {
        RunPhase(Phase::Update, dt, serviceProvider);
    }

    void SystemManager::PostUpdate(const float dt, const skr::Arc<skr::ServiceProvider>& serviceProvider)
    {
        RunPhase(Phase::PostUpdate, dt, serviceProvider);
    }

} // namespace FREYR_NAMESPACE
