#pragma once

#include "Freyr/Base/Component.hpp"
#include "Freyr/Meta/Iteration.hpp"

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
        bool               operator==(const Signature& other) const;

        template <typename TComponent>
            requires IsComponent<TComponent>
        void AddComponent()
        {
            AddComponent(GetComponentId<TComponent>());
        }

        template <typename... Ts>
        void AddComponents()
        {
            meta::forEach(
                [this]<typename T>([[maybe_unused]] T&& c) {
                    using TComp = std::remove_reference_t<T>;
                    AddComponent<TComp>();
                },
                std::make_tuple<>(Ts {}...));
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

        template <typename... Components>
            requires(IsComponent<Components> and ...)
        constexpr static auto Make() -> Signature
        {
            auto signature = Signature {};

            meta::forEach([&signature]<typename TComponent>(TComponent) { signature.AddComponent<TComponent>(); },
                          std::tuple<Components...> {});

            return signature;
        }
      private:
        std::vector<BitSet> mBitSets;
    };
} // namespace FREYR_NAMESPACE