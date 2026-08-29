#pragma once

#include "Freyr/Containers/Archetype.hpp"
#include "Freyr/Containers/Signature.hpp"
#include "Freyr/Core/Filter.hpp"

#include <functional>
#include <unordered_map>
#include <vector>

namespace FREYR_NAMESPACE
{

    struct SignatureHash
    {
        size_t operator()(const Signature& signature) const noexcept { return signature.Hash(); }
    };

    struct FilterEqual
    {
        bool operator()(const Filter& lhs, const Filter& rhs) const noexcept
        {
            return lhs.IncludeSignature() == rhs.IncludeSignature() &&
                   lhs.ExcludeSignature() == rhs.ExcludeSignature();
        }
    };

    struct FilterHash
    {
        size_t operator()(const Filter& filter) const noexcept
        {
            const auto includeHash = filter.IncludeSignature().Hash();
            const auto excludeHash = filter.ExcludeSignature().Hash();
            return includeHash ^ (excludeHash + 0x9e3779b9 + (includeHash << 6) + (includeHash >> 2));
        }
    };

    class ArchetypeMatchIndex
    {
      public:
        using BootstrapFn = std::function<std::vector<Archetype*>(const Signature& includeSignature)>;
        using FilterBootstrapFn = std::function<std::vector<Archetype*>(const Filter& filter)>;

        [[nodiscard]] const std::vector<Archetype*>& GetOrBuild(const Signature&  includeSignature,
                                                                 const BootstrapFn& bootstrap);

        [[nodiscard]] const std::vector<Archetype*>& GetOrBuildFilter(const Filter&           filter,
                                                                        const FilterBootstrapFn& bootstrap);

        void OnArchetypeAdded(Archetype* archetype);

      private:
        [[nodiscard]] static bool ContainsArchetype(const std::vector<Archetype*>& archetypes,
                                                    Archetype*                       archetype);

        static void AppendUnique(std::vector<Archetype*>& archetypes, Archetype* archetype);

        std::unordered_map<Signature, std::vector<Archetype*>, SignatureHash> mByInclude;
        std::unordered_map<Filter, std::vector<Archetype*>, FilterHash, FilterEqual> mByFilter;
    };

} // namespace FREYR_NAMESPACE
