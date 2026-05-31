#pragma once

#include "Freyr/Containers/Signature.hpp"
#include "Freyr/Containers/Archetype.hpp"

namespace FREYR_NAMESPACE
{

    /**
     * @brief Filters archetypes based on component inclusion and exclusion rules.
     *
     * A QueryFilter defines which archetypes are relevant to a query by specifying
     * required components (include) and prohibited components (exclude).
     */
    class Filter
    {
      public:
        Filter() {}

        Filter(const Filter&) = default;

        /**
         * @brief Adds component types to the inclusion filter.
         *
         * @tparam Ts  Component types that matching archetypes must have
         *
         * @note An archetype must have ALL included components to match.
         */
        template <typename... Ts>
            requires(IsComponent<Ts> and ...)
        void Including()
        {
            mIncludeSignature = {};
            mIncludeSignature.AddComponents<Ts...>();
        }

        /**
         * @brief Adds component types to the exclusion filter.
         *
         * @tparam Ts  Component types that matching archetypes must NOT have
         *
         * @note An archetype is rejected if it has ANY of the excluded components.
         */
        template <typename... Ts>
            requires(IsComponent<Ts> and ...)
        void Excluding()
        {
            mExcludeSignature.AddComponents<Ts...>();
        }

        bool MatchArchetype(const Archetype* archetype) const
        {
            const auto& archetypeSignature = archetype->GetSignature();

            if (!mExcludeSignature.IsEmpty())
            {
                if (mExcludeSignature.Match(archetypeSignature))
                    return false;
            }

            if (!mIncludeSignature.IsEmpty())
            {
                return mIncludeSignature.Match(archetypeSignature);
            }

            return true;
        }

      private:
        Signature mIncludeSignature;
        Signature mExcludeSignature;
    };

} // namespace FREYR_NAMESPACE
