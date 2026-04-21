#include <Freyr/Core/SystemManager.hpp>

#include <Freyr/Core/Profiling.hpp>

namespace FREYR_NAMESPACE
{
    void SystemManager::Accumulate(float dt)
    {
        mReadyPipelines.clear();

        for (auto& pipeline : mPipelines)
        {
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

    void SystemManager::PreUpdate(const float dt, const Ref<skr::ServiceProvider>& serviceProvider)
    {
        FREYR_TRACE_BEGIN("FREYR", "Schedule: PreUpdate", perfetto::Track(0, perfetto::ProcessTrack::Current()));

        for (auto pipeline : mReadyPipelines)
        {
            FREYR_TRACE_BEGIN("FREYR", pipeline->Name.data(), perfetto::Track(0, perfetto::ProcessTrack::Current()));

            const float effectiveDt = pipeline->Rate == 0.0f ? dt : pipeline->Rate;

            for (const auto id : pipeline->Systems)
            {
                FREYR_TRACE_BEGIN("FREYR", GetSystemLabel(id).data(),
                                  perfetto::Track(0, perfetto::ProcessTrack::Current()));
                auto* system = GetSystem(id, serviceProvider).get();

                system->PreUpdate(effectiveDt);
                FREYR_TRACE_END("FREYR", perfetto::Track(0));
            }

            FREYR_TRACE_END("FREYR", perfetto::Track(0));
        }

        mQueryAggregator->Flush();
        FREYR_TRACE_END("FREYR", perfetto::Track(0));
    }

    void SystemManager::Update(const float dt, const Ref<skr::ServiceProvider>& serviceProvider)
    {
        FREYR_TRACE_BEGIN("FREYR", "Schedule: Update", perfetto::Track(0, perfetto::ProcessTrack::Current()));

        for (auto pipeline : mReadyPipelines)
        {
            FREYR_TRACE_BEGIN("FREYR", pipeline->Name.data(), perfetto::Track(0, perfetto::ProcessTrack::Current()));

            const float effectiveDt = pipeline->Rate == 0.0f ? dt : pipeline->Rate;

            for (const auto id : pipeline->Systems)
            {
                FREYR_TRACE_BEGIN("FREYR", GetSystemLabel(id).data(),
                                  perfetto::Track(0, perfetto::ProcessTrack::Current()));
                auto* system = GetSystem(id, serviceProvider).get();

                system->Update(effectiveDt);
                FREYR_TRACE_END("FREYR", perfetto::Track(0));
            }

            FREYR_TRACE_END("FREYR", perfetto::Track(0));
        }

        mQueryAggregator->Flush();

        FREYR_TRACE_END("FREYR", perfetto::Track(0));
    }

    void SystemManager::PostUpdate(const float dt, const Ref<skr::ServiceProvider>& serviceProvider)
    {
        FREYR_TRACE_BEGIN("FREYR", "Schedule: PostUpdate", perfetto::Track(0, perfetto::ProcessTrack::Current()));

        for (auto pipeline : mReadyPipelines)
        {
            FREYR_TRACE_BEGIN("FREYR", pipeline->Name.data(), perfetto::Track(0, perfetto::ProcessTrack::Current()));

            const float effectiveDt = pipeline->Rate == 0.0f ? dt : pipeline->Rate;

            for (const auto id : pipeline->Systems)
            {
                FREYR_TRACE_BEGIN("FREYR", GetSystemLabel(id).data(),
                                  perfetto::Track(0, perfetto::ProcessTrack::Current()));
                auto* system = GetSystem(id, serviceProvider).get();

                system->PostUpdate(effectiveDt);
                FREYR_TRACE_END("FREYR", perfetto::Track(0));
            }

            FREYR_TRACE_END("FREYR", perfetto::Track(0));
        }

        mQueryAggregator->Flush();

        FREYR_TRACE_END("FREYR", perfetto::Track(0));
    }

} // namespace FREYR_NAMESPACE
