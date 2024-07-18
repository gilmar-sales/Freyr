#pragma once

#include "Freyr/Base/Component.hpp"

namespace FREYR_NAMESPACE
{
    class Signature
    {
        using BitSet = std::bitset<128>;

      public:
        Signature()  = default;
        ~Signature() = default;

        bool Match(const Signature& other) const;
        bool operator==(const Signature& other) const;

        template <typename TComponent>
            requires IsComponent<TComponent>
        void AddComponent()
        {
            auto componentId = GetComponentId<TComponent>();
            auto bitSetIndex = componentId / 128;

            while (bitSetIndex + 1 > mBitSets.size())
            {
                mBitSets.push_back({});
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
            [&signature](auto t) { signature.AddComponent<decltype(t)>(); },
            std::tuple<Components...> {});

        return signature;
    }
} // namespace FREYR_NAMESPACE