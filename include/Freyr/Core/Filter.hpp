#pragma once

#include "Freyr/Base/Tags.hpp"
#include "Freyr/Containers/Archetype.hpp"
#include "Freyr/Containers/Signature.hpp"

#include <vector>

namespace FREYR_NAMESPACE
{

    class Filter
    {
      public:
        Filter()
        {
            Excluding<Disabled>();
            Excluding<Prefab>();
        }

        Filter(const Filter&) = default;

        template <typename... Ts>
            requires(IsComponent<Ts> and ...)
        void Including()
        {
            mIncludeSignature = {};
            mIncludeSignature.AddComponents<Ts...>();
        }

        template <typename... Ts>
            requires(IsComponent<Ts> and ...)
        void Excluding()
        {
            mExcludeSignature.AddComponents<Ts...>();
        }

        void IncludingDisabled() { mExcludeSignature.RemoveComponent<Disabled>(); }

        void IncludingPrefabs() { mExcludeSignature.RemoveComponent<Prefab>(); }

        template <typename... Ts>
            requires(IsComponent<Ts> and ...)
        void Changed()
        {
            (mChangedIds.push_back(GetComponentId<Ts>()), ...);
        }

        template <typename... Ts>
            requires(IsComponent<Ts> and ...)
        void Added()
        {
            (mAddedIds.push_back(GetComponentId<Ts>()), ...);
        }

        template <typename... Ts>
            requires(IsComponent<Ts> and ...)
        void Removed()
        {
            (mRemovedIds.push_back(GetComponentId<Ts>()), ...);
        }

        [[nodiscard]] bool HasChangeFilters() const
        {
            return !mChangedIds.empty() || !mAddedIds.empty();
        }

        [[nodiscard]] bool HasRemovedFilters() const { return !mRemovedIds.empty(); }

        [[nodiscard]] const std::vector<ComponentId>& ChangedIds() const { return mChangedIds; }

        [[nodiscard]] const std::vector<ComponentId>& AddedIds() const { return mAddedIds; }

        [[nodiscard]] const std::vector<ComponentId>& RemovedIds() const { return mRemovedIds; }

        bool MatchArchetype(const Archetype* archetype) const
        {
            const auto& archetypeSignature = archetype->GetSignature();

            if (!mExcludeSignature.IsEmpty())
            {
                if (mExcludeSignature.Intersects(archetypeSignature))
                    return false;
            }

            if (!mIncludeSignature.IsEmpty())
            {
                return mIncludeSignature.Match(archetypeSignature);
            }

            return true;
        }

        [[nodiscard]] const Signature& IncludeSignature() const { return mIncludeSignature; }

        [[nodiscard]] const Signature& ExcludeSignature() const { return mExcludeSignature; }

      private:
        Signature                 mIncludeSignature;
        Signature                 mExcludeSignature;
        std::vector<ComponentId>  mChangedIds;
        std::vector<ComponentId>  mAddedIds;
        std::vector<ComponentId>  mRemovedIds;
    };

} // namespace FREYR_NAMESPACE
