#pragma once

#include "Freyr/Base/System.hpp"

namespace FREYR_NAMESPACE
{
    struct Pipeline
    {
        std::string_view Name;
        float            Rate;
        float            Accumulator;
        std::vector<SystemId> Systems;

        explicit Pipeline(std::string_view name, float rate)
            : Name(name), Rate(rate), Accumulator(0.0f)
        {
        }
    };
} // namespace FREYR_NAMESPACE