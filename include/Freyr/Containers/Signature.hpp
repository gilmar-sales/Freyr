#pragma once

#include "Freyr/Base/Component.hpp"
#include "Freyr/Meta/Iteration.hpp"

namespace FREYR_NAMESPACE
{
    class Signature
    {
        using BitSet = std::bitset<128>;

      public:
        Signature()  = default;
        ~Signature() = default;

        [[nodiscard]] bool Match(const Signature& other) const;
        bool               operator==(const Signature& other) const;

        template <typename TComponent>
            requires IsComponent<TComponent>
        void AddComponent()
        {
            auto componentId = GetComponentId<TComponent>();
            auto bitSetIndex = componentId / 128;

            auto growth = mBitSets.size() - bitSetIndex - 1;

            for (int i = 0; i < growth; ++i)
            {
                mBitSets.emplace_back();
            }

            mBitSets[bitSetIndex][componentId % 128] = true;
        }

        template <typename TComponent>
            requires IsComponent<TComponent>
        void RemoveComponent()
        {
            auto componentId = GetComponentId<TComponent>();
            auto bitSetIndex = componentId / 128;
            if (bitSetIndex < mBitSets.size())
            {
                mBitSets[bitSetIndex][componentId % 128] = false;
            }
        }

      private:
        std::vector<BitSet> mBitSets;
    };

    template <typename... Components>
        requires(IsComponent<Components> and ...)
    constexpr static auto MakeSignature() -> Signature
    {
        auto signature = Signature {};

        meta::forEach(
            [&signature]<typename TComponent>(TComponent component) {
                signature.AddComponent<TComponent>();
            },
            std::tuple<Components...> {});

        return signature;
    }
} // namespace FREYR_NAMESPACE