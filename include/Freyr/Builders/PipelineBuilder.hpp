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
        explicit PipelineBuilder(const int32_t                                pipelineId,
                                 std::vector<Action<skr::ServiceCollection>>& serviceCollectionFunctions,
                                 std::vector<Action<SystemManager>>&          systemManagerFunctions) :
            mPipelineId(pipelineId), mName(std::format("Pipeline {}", pipelineId)), mRate(0.0f),
            mSystemManagerFunctions(systemManagerFunctions), mServiceCollectionFunctions(serviceCollectionFunctions)
        {
        }

        PipelineBuilder& WithName(const std::string_view name)
        {
            mName = name;

            return *this;
        }

        PipelineBuilder& WithRate(const float rate)
        {
            mRate = rate;
            return *this;
        }

        template <typename T>
            requires IsSystem<T>
        PipelineBuilder& WithSystem()
        {
            mSystemManagerFunctions.emplace_back([pipelineId = mPipelineId](SystemManager& systemManager) {
                systemManager.RegisterSystem<T>(pipelineId);
            });

            mServiceCollectionFunctions.emplace_back([](skr::ServiceCollection& services) {
                services.AddSingleton<T>();
            });

            return *this;
        }

      private:
        friend class FreyrExtension;

        PipelineConfig Build() const { return { .Name = mName, .Rate = mRate, .PipelineId = mPipelineId }; }

        int32_t                                      mPipelineId;
        std::string                                  mName;
        float                                        mRate;
        std::vector<Action<skr::ServiceCollection>>& mServiceCollectionFunctions;
        std::vector<Action<SystemManager>>&          mSystemManagerFunctions;
    };
} // namespace FREYR_NAMESPACE