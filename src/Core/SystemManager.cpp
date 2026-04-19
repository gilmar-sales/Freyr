#include <Freyr/Core/SystemManager.hpp>

#include <Freyr/Core/Profiling.hpp>

namespace FREYR_NAMESPACE
{
    void SystemManager::Update(const float dt, const Ref<skr::ServiceProvider>& serviceProvider)
    {
        FREYR_TRACE("FREYR", "Schedule: RunPipelines");

        for (auto& pipeline : mPipelines)
        {
            const float effectiveDt = (pipeline.Rate == 0.0f) ? dt : pipeline.Rate;

            if (pipeline.Rate > 0.0f)
            {
                pipeline.Accumulator += dt;
            }

            for (auto const& id : pipeline.Systems)
            {
                FREYR_TRACE("FREYR", GetSystemLabel(id).data());
                auto* system = GetSystem(id, serviceProvider).get();

                system->PreUpdate(effectiveDt);

                if (pipeline.Rate == 0.0f || pipeline.Accumulator >= pipeline.Rate)
                {
                    system->Update(effectiveDt);
                }

                system->PostUpdate(effectiveDt);
            }

            if (pipeline.Rate > 0.0f && pipeline.Accumulator >= pipeline.Rate)
            {
                pipeline.Accumulator -= pipeline.Rate;
            }
        }
    }
} // namespace FREYR_NAMESPACE
