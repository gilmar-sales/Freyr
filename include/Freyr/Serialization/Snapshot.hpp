#pragma once

#include "Freyr/Core/Registry.hpp"

#include <istream>
#include <ostream>
#include <stdexcept>
#include <string>

namespace FREYR_NAMESPACE
{
    class SnapshotError : public std::runtime_error
    {
      public:
        using std::runtime_error::runtime_error;
    };

    class SnapshotWriter
    {
      public:
        void Save(Registry& registry, std::ostream& out) const;
    };

    class SnapshotReader
    {
      public:
        void Load(Registry& registry, std::istream& in) const;
    };
} // namespace FREYR_NAMESPACE
