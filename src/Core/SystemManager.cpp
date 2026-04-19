#include <Freyr/Core/SystemManager.hpp>

#include <Freyr/Core/Profiling.hpp>

namespace FREYR_NAMESPACE
{
    void SystemManager::Accumulate(float dt)
    {
        mReadyPipelines.clear();

        for (auto& pipeline : mPipelines)
        {
            if (pipeline.Rate > 0.0f)
            {
                pipeline.Accumulator += dt;
            }

            if (pipeline.Accumulator >= pipeline.Rate)
            {
                pipeline.Accumulator += dt;
                mReadyPipelines.push_back(&pipeline);
            }

            if (pipeline.Rate > 0.0f && pipeline.Accumulator >= pipeline.Rate)
            {
                pipeline.Accumulator -= pipeline.Rate;
            }
        }
    }

    void SystemManager::PreUpdate(const float dt, const Ref<skr::ServiceProvider>& serviceProvider)
    {
        FREYR_TRACE("FREYR", "Schedule: PreUpdate");

        for (auto pipeline : mReadyPipelines)
        {
            FREYR_TRACE("FREYR", pipeline->Name.data());
            const float effectiveDt = pipeline->Rate == 0.0f ? dt : pipeline->Rate;

            for (const auto id : pipeline->Systems)
            {
                FREYR_TRACE("FREYR", GetSystemLabel(id).data());
                auto* system = GetSystem(id, serviceProvider).get();

                system->PreUpdate(effectiveDt);
            }
        }
    }

    void SystemManager::Update(const float dt, const Ref<skr::ServiceProvider>& serviceProvider)
    {
        FREYR_TRACE("FREYR", "Schedule: Update");

        for (auto pipeline : mReadyPipelines)
        {
            FREYR_TRACE("FREYR", pipeline->Name.data());
            const float effectiveDt = pipeline->Rate == 0.0f ? dt : pipeline->Rate;

            for (const auto id : pipeline->Systems)
            {
                FREYR_TRACE("FREYR", GetSystemLabel(id).data());
                auto* system = GetSystem(id, serviceProvider).get();

                system->Update(effectiveDt);
            }
        }
    }

    void SystemManager::PostUpdate(const float dt, const Ref<skr::ServiceProvider>& serviceProvider)
    {
        FREYR_TRACE("FREYR", "Schedule: PostUpdate");

        for (auto pipeline : mReadyPipelines)
        {
            FREYR_TRACE("FREYR", pipeline->Name.data());
            const float effectiveDt = pipeline->Rate == 0.0f ? dt : pipeline->Rate;

            for (const auto id : pipeline->Systems)
            {
                FREYR_TRACE("FREYR", GetSystemLabel(id).data());
                auto* system = GetSystem(id, serviceProvider).get();

                system->PostUpdate(effectiveDt);
            }
        }
    }

} // namespace FREYR_NAMESPACE
