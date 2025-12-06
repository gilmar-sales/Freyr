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
            AddComponent(GetComponentId<TComponent>());
        }

        template <typename... Ts>
        void AddComponents()
        {
            meta::forEach(
                [this](auto&& c) {
                    using T = std::remove_reference_t<decltype(c)>;
                    AddComponent<T>();
                },
                std::make_tuple<>(Ts {}...));
        }

        void AddComponent(ComponentId componentId)
        {
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

        meta::forEach([&signature]<typename TComponent>(TComponent) { signature.AddComponent<TComponent>(); },
                      std::tuple<Components...> {});

        return signature;
    }
} // namespace FREYR_NAMESPACE