#pragma once

#include "Freyr/Builders/FreyrOptionsBuilder.hpp"
#include "Freyr/Builders/PipelineBuilder.hpp"
#include "Freyr/Core/Registry.hpp"
#include <Skirnir/Skirnir.hpp>

namespace FREYR_NAMESPACE
{
    class SystemManager;

    class FreyrExtension : public skr::IExtension
    {
      public:
        /**
         * @brief Registers a component type with the Freyr extension.
         *
         * @tparam T  Component type (must satisfy IsComponent)
         * @return Reference to this FreyrExtension for chaining
         *
         * @note Registered components are available to all Registries created by this extension.
         */
        template <typename T>
            requires IsComponent<T>
        FreyrExtension& WithComponent()
        {
            mComponentManagerFunctions.push_back([this](ComponentManager& componentManager) {
                componentManager.RegisterComponent<T>();
            });

            return *this;
        }

        /**
         * @brief Configures Freyr options via a callback with a FreyrOptionsBuilder.
         *
         * @param func  Callback receiving a FreyrOptionsBuilder to configure options
         * @return Reference to this FreyrExtension for chaining
         *
         * @see FreyrOptionsBuilder for available configuration options.
         */
        FreyrExtension& WithOptions(const std::function<void(FreyrOptionsBuilder&)>& func)
        {
            func(mFreyrOptionsBuilder);

            return *this;
        }

        /**
         * @brief Configures a pipeline with systems and execution strategy.
         *
         * @param callback  Callback receiving a PipelineBuilder to configure the pipeline
         * @return Reference to this FreyrExtension for chaining
         *
         * @note Multiple pipelines can be configured; each receives a unique pipelineId.
         */
        FreyrExtension& WithPipeline(std::function<void(PipelineBuilder&)> callback)
        {
            const int32_t pipelineId = static_cast<int32_t>(mPipelineConfigs.size());

            PipelineBuilder builder(pipelineId, mServiceCollectionFunctions, mSystemManagerFunctions);
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
