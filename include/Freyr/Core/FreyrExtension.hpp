#pragma once

#include "Freyr/Pch.hpp"

#include "Freyr/Builders/FreyrOptionsBuilder.hpp"
#include "Freyr/Builders/PipelineBuilder.hpp"
#include "Freyr/Core/Registry.hpp"
#include "Freyr/Hierarchy/HierarchyComponents.hpp"
#include "Freyr/Hierarchy/HierarchyPropagationPolicy.hpp"
#include "Freyr/Hierarchy/HierarchyPropagationSystem.hpp"
#include <Skirnir/Skirnir.hpp>

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

        FreyrExtension& WithHierarchy()
        {
            return WithComponent<ChildOf>().WithComponent<ParentDepth>();
        }

        template <HierarchyPropagationPolicy P>
        FreyrExtension& WithHierarchyPropagation()
        {
            WithHierarchy();
            WithComponent<typename P::Local>();
            WithComponent<typename P::World>();
            return WithPipeline([](PipelineBuilder& pipeline) {
                pipeline.WithName("HierarchyPropagation")
                    .WithSystem<HierarchyPropagationSystem<P>>();
            });
        }

        FreyrExtension& WithOptions(const std::function<void(FreyrOptionsBuilder&)>& func)
        {
            func(mFreyrOptionsBuilder);

            return *this;
        }

        FreyrExtension& WithPipeline(std::function<void(PipelineBuilder&)> callback)
        {
            const int32_t pipelineId = static_cast<int32_t>(mPipelineConfigs.size());

            PipelineBuilder builder(
                pipelineId, mServiceCollectionFunctions, mSystemManagerFunctions);
            callback(builder);

            mPipelineConfigs.push_back(builder.Build());

            return *this;
        }

      protected:
        void ConfigureServices(skr::ServiceCollection& services) override;
        void UseServices(skr::ServiceProvider& serviceProvider) override;

      private:
        std::vector<Action<skr::ServiceCollection>> mServiceCollectionFunctions;
        std::vector<Action<SystemManager>>          mSystemManagerFunctions;
        std::vector<Action<ComponentManager>>       mComponentManagerFunctions;
        std::vector<PipelineConfig>                 mPipelineConfigs;

        FreyrOptionsBuilder mFreyrOptionsBuilder;

        friend class SystemManager;
    };
} // namespace FREYR_NAMESPACE
