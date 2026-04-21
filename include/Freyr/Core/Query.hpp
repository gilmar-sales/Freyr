#pragma once

#include "Freyr/Containers/Signature.hpp"
#include "Freyr/Core/ComponentManager.hpp"

namespace FREYR_NAMESPACE
{
    class QueryAggregator;
    using QueryAction = Action<ArchetypeChunk>;

    class QueryFilter
    {
      public:
        QueryFilter() {}

        QueryFilter(const QueryFilter&) = default;

        template <typename... Ts>
            requires(IsComponent<Ts> and ...)
        void Including()
        {
            mIncludeSignature.AddComponents<Ts...>();
        }

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

    struct PendingQuery
    {
        std::string label;
        QueryFilter filter;
        QueryAction action;
    };

    class Query
    {
      public:
        explicit Query(const Ref<ComponentManager>& componentManager, const Ref<QueryAggregator>& queryAggregator);
        virtual ~Query();

        template <typename... Ts>
            requires(IsComponent<Ts> and ...)
        Query& All()
        {
            mQueryFilter.Including<Ts...>();
            return *this;
        }

        template <typename... Ts>
            requires(IsComponent<Ts> and ...)
        Query& Excluding()
        {
            mQueryFilter.Excluding<Ts...>();
            return *this;
        }

        template <typename... Ts>
            requires(IsComponent<Ts> and ...)
        Query& Each(auto&& action)
        {
            mAction = [action = std::forward<decltype(action)>(action)](ArchetypeChunk& chunk) {
                chunk.ForEach<Ts...>(skr::type_name<decltype(action)>(), action);
            };

            return All<Ts...>();
        }

        template <typename... Ts>
            requires(IsComponent<Ts> and ...)
        auto Transform(auto&& callback) -> std::vector<decltype(callback(*(new Entity {}), *(new Ts {})...))>
        {
            constexpr bool hasEntity   = std::is_invocable_v<decltype(callback), Entity, Ts&...>;
            constexpr bool hasNoEntity = std::is_invocable_v<decltype(callback), Ts&...>;
            static_assert(hasEntity || hasNoEntity, "Callback must accept either (Entity, Ts...) or (Ts...)");

            using ResultType = decltype(callback(*(new Entity {}), *(new Ts {})...));
            auto results     = std::vector<ResultType>();

            mComponentManager->ForEachArchetype([&](Archetype* archetype) {
                if (!mQueryFilter.MatchArchetype(archetype))
                    return;

                archetype->ForEach<Ts...>(mLabel.data(), [&](Entity entity, Ts&... components) mutable {
                    if constexpr (hasEntity)
                    {
                        results.push_back(callback(entity, components...));
                    }
                    else
                    {
                        results.push_back(callback(components...));
                    }
                });
            });

            return results;
        }

        template <typename... Ts>
            requires(IsComponent<Ts> and ...)
        auto Reduce(auto&& callback, auto seed) -> decltype(seed)
        {
            using ResultType       = decltype(seed);
            ResultType accumulator = seed;

            mComponentManager->ForEachArchetype([&](Archetype* archetype) {
                if (!mQueryFilter.MatchArchetype(archetype))
                    return;

                archetype->ForEach<Ts...>(mLabel.data(), [&](Ts&... components) {
                    accumulator = callback(accumulator, components...);
                });
            });

            return accumulator;
        }

        // TODO: implement dedicated First in Archetype and ArchetypeChunk to stop when the first is found
        // template <typename... Ts>
        //     requires(IsComponent<Ts> and ...)
        // std::optional<Entity> First()
        // {
        //     for (auto&& archetype : mComponentManager->mArchetypes)
        //     {
        //         if (!mQueryFilter.MatchArchetype(archetype.get()))
        //             continue;
        //
        //         std::optional<Entity> result;
        //         archetype->ForEach<Ts...>(mLabel.data(), [&](Entity entity, Ts&... components) { result = entity; });
        //
        //         if (result.has_value())
        //             return result;
        //     }
        //
        //     return std::nullopt;
        // }

        template <typename... Ts>
            requires(IsComponent<Ts> and ...)
        auto Iterate() -> std::vector<std::tuple<Entity, Ts...>>
        {
            using ResultType = std::tuple<Entity, Ts...>;
            auto results     = std::vector<ResultType>();

            mComponentManager->ForEachArchetype([&](Archetype* archetype) {
                if (!mQueryFilter.MatchArchetype(archetype))
                    return;

                archetype->ForEach<Ts...>(mLabel.data(), [&](Entity entity, Ts&... components) {
                    results.emplace_back(entity, components...);
                });
            });

            return results;
        }

        template <typename... Ts>
            requires(IsComponent<Ts> and ...)
        std::size_t Count()
        {
            std::size_t count = 0;

            mComponentManager->ForEachArchetype([&](const Archetype* archetype) {
                if (!mQueryFilter.MatchArchetype(archetype))
                    return;

                count += archetype->Count();
            });

            return count;
        }

        Query& Label(std::string_view name)
        {
            mLabel = std::string(name);
            return *this;
        }

        void Run() const;

        void Schedule() const;

      protected:
        ComponentManager& GetComponentManager() const { return *mComponentManager; }

        template <typename... Ts>
            requires(IsComponent<Ts> and ...)
        std::vector<Archetype*> GetMatchingArchetypes() const
        {
            auto archetypes = std::vector<Archetype*>();
            auto signature  = Signature::Make<Ts...>();

            mComponentManager->ForEachArchetype([&](Archetype* archetype) {
                if (signature.Match(archetype->GetSignature()))
                {
                    archetypes.push_back(archetype);
                }
            });

            return archetypes;
        }

        template <typename... Ts>
            requires(IsComponent<Ts> and ...)
        void ForEachInternal(auto&& callback)
        {
            mComponentManager->ForEachArchetype([&](Archetype* archetype) {
                if (!mQueryFilter.MatchArchetype(archetype))
                    return;

                archetype->ForEach<Ts...>(mLabel.data(), callback);
            });
        }

        virtual void OnExecute() {}
        virtual void OnFlush() {}

      private:
        Ref<ComponentManager> mComponentManager;
        Ref<QueryAggregator>  mQueryAggregator;

        QueryFilter mQueryFilter;
        QueryAction mAction;
        std::string mLabel;
        Ref<void>   mContext;
    };

} // namespace FREYR_NAMESPACE