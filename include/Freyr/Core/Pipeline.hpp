#pragma once

#include "Freyr/Base/System.hpp"

#include <span>

namespace FREYR_NAMESPACE
{
    struct Pipeline
    {
        std::string           Name;
        float                 Rate;
        float                 Accumulator;
        bool                  Enabled = true;
        std::vector<SystemId> Systems;

        explicit Pipeline(std::string_view name, float rate) :
            Name(name), Rate(rate), Accumulator(0.0f), Enabled(true)
        {
        }
    };

    struct PipelineView
    {
        int32_t                   Id;
        std::string_view          Name;
        float                     Rate;
        bool                      Enabled;
        std::span<const SystemId> Systems;
    };
} // namespace FREYR_NAMESPACE