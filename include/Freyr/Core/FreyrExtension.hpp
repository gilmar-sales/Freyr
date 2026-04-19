#pragma once

#include "Freyr/Builders/FreyrOptionsBuilder.hpp"
#include "Freyr/Builders/PipelineBuilder.hpp"
#include "Freyr/Core/Scene.hpp"
#include "Skirnir/ApplicationBuilder.hpp"

namespace FREYR_NAMESPACE
{
    class SystemManager;

    class FreyrExtension : public skr::IExtension
    {
      public:
        template <typename T>
            requires IsComponent<T>
        FreyrExtension& WithComponent()
        {
            mComponentManagerFunctions.push_back([this](ComponentManager& componentManager) {
                componentManager.RegisterComponent<T>();
            });

            return *this;
        }

        FreyrExtension& WithOptions(const std::function<void(FreyrOptionsBuilder&)>& func)
        {
            func(mFreyrOptionsBuilder);

            return *this;
        }

        FreyrExtension& WithPipeline(std::function<void(PipelineBuilder&)> callback)
        {
            const int32_t pipelineId = static_cast<int32_t>(mPipelineConfigs.size());

            PipelineBuilder builder(pipelineId, mServiceCollectionFunctions, mSystemManagerFunctions);
            callback(builder);

            mPipelineConfigs.push_back(builder.Build());

            return *this;
        }

      private:
        void ConfigureServices(skr::ServiceCollection& services) override;
        void UseServices(skr::ServiceProvider& serviceProvider) override;

        std::vector<Action<skr::ServiceCollection>> mServiceCollectionFunctions;
        std::vector<Action<SystemManager>>          mSystemManagerFunctions;
        std::vector<Action<ComponentManager>>       mComponentManagerFunctions;
        std::vector<PipelineConfig>                 mPipelineConfigs;

        FreyrOptionsBuilder mFreyrOptionsBuilder;

        friend class SystemManager;
    };
} // namespace FREYR_NAMESPACE
