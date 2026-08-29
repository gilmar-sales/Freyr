#include "Freyr/Core/ArchetypeMatchIndex.hpp"

namespace FREYR_NAMESPACE
{

    bool ArchetypeMatchIndex::ContainsArchetype(const std::vector<Archetype*>& archetypes,
                                                Archetype*                       archetype)
    {
        for (const auto* entry : archetypes)
        {
            if (entry == archetype)
                return true;
        }

        return false;
    }

    void ArchetypeMatchIndex::AppendUnique(std::vector<Archetype*>& archetypes, Archetype* archetype)
    {
        if (ContainsArchetype(archetypes, archetype))
            return;

        archetypes.push_back(archetype);
    }

    const std::vector<Archetype*>& ArchetypeMatchIndex::GetOrBuild(const Signature&  includeSignature,
                                                                   const BootstrapFn& bootstrap)
    {
        if (const auto iterator = mByInclude.find(includeSignature); iterator != mByInclude.end())
            return iterator->second;

        auto [inserted, _] = mByInclude.emplace(includeSignature, bootstrap(includeSignature));
        return inserted->second;
    }

    const std::vector<Archetype*>& ArchetypeMatchIndex::GetOrBuildFilter(const Filter&           filter,
                                                                          const FilterBootstrapFn& bootstrap)
    {
        if (const auto iterator = mByFilter.find(filter); iterator != mByFilter.end())
            return iterator->second;

        auto [inserted, _] = mByFilter.emplace(filter, bootstrap(filter));
        return inserted->second;
    }

    void ArchetypeMatchIndex::OnArchetypeAdded(Archetype* archetype)
    {
        const auto& archetypeSignature = archetype->GetSignature();

        for (auto& [includeSignature, archetypes] : mByInclude)
        {
            if (!includeSignature.Match(archetypeSignature))
                continue;

            AppendUnique(archetypes, archetype);
        }

        for (auto& [filter, archetypes] : mByFilter)
        {
            if (!filter.MatchArchetype(archetype))
                continue;

            AppendUnique(archetypes, archetype);
        }
    }

} // namespace FREYR_NAMESPACE
