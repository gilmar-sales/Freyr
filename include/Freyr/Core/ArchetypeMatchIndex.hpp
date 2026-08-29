#pragma once

#include "Freyr/Containers/Archetype.hpp"
#include "Freyr/Containers/Signature.hpp"

#include <functional>
#include <unordered_map>
#include <vector>

namespace FREYR_NAMESPACE
{

    struct SignatureHash
    {
        size_t operator()(const Signature& signature) const noexcept { return signature.Hash(); }
    };

    class ArchetypeMatchIndex
    {
      public:
        using BootstrapFn = std::function<std::vector<Archetype*>(const Signature& includeSignature)>;

        [[nodiscard]] const std::vector<Archetype*>& GetOrBuild(const Signature&  includeSignature,
                                                                 const BootstrapFn& bootstrap);

        void OnArchetypeAdded(Archetype* archetype);

      private:
        [[nodiscard]] static bool ContainsArchetype(const std::vector<Archetype*>& archetypes,
                                                    Archetype*                       archetype);

        static void AppendUnique(std::vector<Archetype*>& archetypes, Archetype* archetype);

        std::unordered_map<Signature, std::vector<Archetype*>, SignatureHash> mByInclude;
    };

} // namespace FREYR_NAMESPACE
