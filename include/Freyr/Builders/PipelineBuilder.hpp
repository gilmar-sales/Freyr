#pragma once

#include <Skirnir/Skirnir.hpp>

#include "Freyr/Core/SystemManager.hpp"

namespace FREYR_NAMESPACE
{

    struct PipelineConfig
    {
        std::string Name;
        float       Rate;
        int32_t     PipelineId;
    };

    class PipelineBuilder
    {
      public:
        /**
         * @brief Constructs a PipelineBuilder for configuring a pipeline.
         *
         * @param pipelineId              Unique identifier for this pipeline
         * @param serviceCollectionFunctions  Reference to shared service collection functions list
         * @param systemManagerFunctions      Reference to shared system manager functions list
         */
        explicit PipelineBuilder(
            const int32_t                                pipelineId,
            std::vector<Action<skr::ServiceCollection>>& serviceCollectionFunctions,
            std::vector<Action<SystemManager>>&          systemManagerFunctions) :
            mPipelineId(pipelineId), mName(std::format("Pipeline {}", pipelineId)), mRate(0.0f),
            mSystemManagerFunctions(systemManagerFunctions),
            mServiceCollectionFunctions(serviceCollectionFunctions)
        {
        }

        /**
         * @brief Sets the human-readable name for this pipeline.
         *
         * @param name  Descriptive name for debugging/profiling
         * @return Reference to this builder for chaining
         */
        PipelineBuilder& WithName(const std::string_view name)
        {
            mName = name;

            return *this;
        }

        /**
         * @brief Sets the update rate (frequency) for this pipeline.
         *
         * @param rate  Desired frequency in Hz; inverse becomes update interval.
         *              Values <= 0 result in every frame execution.
         * @return Reference to this builder for chaining
         */
        PipelineBuilder& WithRate(const float rate)
        {
            mRate = rate > 0.0f ? 1.0f / rate : 0.0f;
            return *this;
        }

        /**
         * @brief Registers a system type to this pipeline.
         *
         * @tparam T  System type (must satisfy IsSystem)
         * @return Reference to this builder for chaining
         *
         * @note The system is also registered as a singleton service via Skirnir's DI container.
         */
        template <typename T>
            requires IsSystem<T>
        PipelineBuilder& WithSystem()
        {
            mSystemManagerFunctions.emplace_back(
                [pipelineId = mPipelineId](SystemManager& systemManager) {
                    systemManager.RegisterSystem<T>(pipelineId);
                });

            mServiceCollectionFunctions.emplace_back([](skr::ServiceCollection& services) {
                services.AddSingleton<T>();
            });

            return *this;
        }

      private:
        friend class FreyrExtension;

        /**
         * @brief Finalizes the pipeline configuration.
         *
         * @return PipelineConfig containing name, rate, and pipelineId
         */
        PipelineConfig Build() const
        {
            return { .Name = mName, .Rate = mRate, .PipelineId = mPipelineId };
        }

        int32_t                                      mPipelineId;
        std::string                                  mName;
        float                                        mRate;
        std::vector<Action<skr::ServiceCollection>>& mServiceCollectionFunctions;
        std::vector<Action<SystemManager>>&          mSystemManagerFunctions;
    };
} // namespace FREYR_NAMESPACE