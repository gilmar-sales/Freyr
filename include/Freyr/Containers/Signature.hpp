#pragma once

#include "Freyr/Base/Component.hpp"

namespace FREYR_NAMESPACE
{
    class Signature
    {
        static constexpr size_t BITSET_SIZE  = 128;
        static constexpr size_t BITSET_MASK  = BITSET_SIZE - 1;
        static constexpr size_t BITSET_SHIFT = std::countr_zero(BITSET_SIZE);

        using BitSet = std::bitset<BITSET_SIZE>;

      public:
        Signature()  = default;
        ~Signature() = default;

        [[nodiscard]] bool Match(const Signature& other) const;
        [[nodiscard]] bool Intersects(const Signature& other) const;
        bool               operator==(const Signature& other) const;

        [[nodiscard]] size_t Hash() const;

        bool IsEmpty() const
        {
            for (auto&& bitSet : mBitSets)
            {
                if (bitSet.any())
                    return false;
            }

            return true;
        }

        template <typename... Ts>
            requires(IsComponent<Ts> and ...)
        void AddComponents()
        {
            (([&] {
                 using TComponent = std::remove_reference_t<Ts>;

                 AddComponent<TComponent>();
             }()),
             ...);
        }

        template <typename TComponent>
            requires IsComponent<TComponent>
        void AddComponent()
        {
            AddComponent(GetComponentId<TComponent>());
        }

        void AddComponent(const ComponentId componentId)
        {
            const auto bitSetIndex = componentId >> BITSET_SHIFT;
            const auto bitOffset   = componentId & BITSET_MASK;

            if (bitSetIndex + 1 > mBitSets.size())
            {
                mBitSets.resize(bitSetIndex + 1);
            }

            mBitSets[bitSetIndex][bitOffset] = true;
        }

        template <typename... Ts>
            requires(IsComponent<Ts> and ...)
        void RemoveComponents()
        {
            (([&] {
                 using TComponent = std::remove_reference_t<Ts>;

                 RemoveComponent<TComponent>();
             }()),
             ...);
        }

        template <typename TComponent>
            requires IsComponent<TComponent>
        void RemoveComponent()
        {
            RemoveComponent(GetComponentId<TComponent>());
        }

        void RemoveComponent(const ComponentId componentId)
        {
            const auto bitSetIndex = componentId >> BITSET_SHIFT;
            const auto bitOffset   = componentId & BITSET_MASK;

            if (bitSetIndex < mBitSets.size())
            {
                mBitSets[bitSetIndex][bitOffset] = false;
            }
        }

        template <typename... Ts>
            requires(IsComponent<Ts> and ...)
        constexpr static auto Make() -> Signature
        {
            auto signature = Signature {};

            (([&] {
                 using TComponent = std::remove_reference_t<Ts>;

                 signature.AddComponent<TComponent>();
             }()),
             ...);

            return signature;
        }

      private:
        std::vector<BitSet> mBitSets;
    };
} // namespace FREYR_NAMESPACE