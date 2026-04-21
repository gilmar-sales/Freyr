#pragma once

#include "Freyr/Containers/Signature.hpp"
#include "Freyr/Core/ComponentManager.hpp"

namespace FREYR_NAMESPACE
{
    class QueryAggregator;
    using QueryAction = Action<ArchetypeChunk>;

    /**
     * @brief Filters archetypes based on component inclusion and exclusion rules.
     *
     * A QueryFilter defines which archetypes are relevant to a query by specifying
     * required components (include) and prohibited components (exclude).
     */
    class QueryFilter
    {
      public:
        QueryFilter() {}

        QueryFilter(const QueryFilter&) = default;

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

    /**
     * @brief Pending query entry for deferred execution via QueryAggregator.
     */
    struct PendingQuery
    {
        QueryFilter filter;
        QueryAction action;
    };

    /**
     * @brief Queries entities based on component signatures with filtering support.
     *
     * Query provides a fluent API for filtering entities by their component composition.
     * It supports include/exclude filters and various terminal operations for collecting
     * or processing matching entities.
     *
     * @note Query instances are typically obtained from Scene::CreateQuery() and
     *       should not be stored long-term as they hold references to ComponentManager.
     */
    class Query
    {
      public:
        /**
         * @brief Constructs a Query bound to a ComponentManager and QueryAggregator.
         *
         * @param componentManager  Reference to the scene's component manager
         * @param queryAggregator   Reference to the scene's query aggregator
         */
        explicit Query(const Ref<ComponentManager>& componentManager, const Ref<QueryAggregator>& queryAggregator);
        virtual ~Query();

        /**
         * @brief Adds component types to the exclusion filter.
         *
         * @tparam Ts  Component types to exclude (all must satisfy IsComponent)
         * @return Reference to this Query for chaining
         *
         * @note Entities with any of the specified components will be excluded from query results.
         */
        template <typename... Ts>
            requires(IsComponent<Ts> and ...)
        Query& Excluding()
        {
            mQueryFilter.Excluding<Ts...>();
            return *this;
        }

        template <typename... Ts>
            requires(IsComponent<Ts> and ...)
        auto Transform(auto&& callback)
            -> std::vector<decltype(callback(std::declval<Entity>(), std::declval<Ts&>()...))>
        {
            All<Ts...>();

            constexpr bool hasEntity   = std::is_invocable_v<decltype(callback), Entity, Ts&...>;
            constexpr bool hasNoEntity = std::is_invocable_v<decltype(callback), Ts&...>;
            static_assert(hasEntity || hasNoEntity, "Callback must accept either (Entity, Ts...) or (Ts...)");

            using ResultType = decltype(callback(std::declval<Entity>(), std::declval<Ts&>()...));
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

        /**
         * @brief Maps each entity to a value and returns a vector of results.
         *
         * @tparam Ts  Component types to filter by
         * @param f             Transform function returning a value for each entity
         * @return Vector of transformed values in entity order
         */
        template <typename... Ts>
            requires(IsComponent<Ts> and ...)
        auto Map(auto&& f) -> std::vector<decltype(callback(std::declval<Entity>(), std::declval<Ts&>()...))>
        {
            auto count = Count<Ts...>();

            using ResultType = decltype(callback(std::declval<Entity>(), std::declval<Ts&>()...));
            auto results     = std::vector<ResultType>(count);

            Entity index = count;

            for (auto&& archetype : mComponentManager->mArchetypes)
            {
                if (mQueryFilter.MatchArchetype(archetype.get()))
                {
                    index -= archetype->Count();
                    archetype->Map<Ts...>(f, index, results);
                }
            }

            return results;
        }

        /**
         * @brief Accumulates values across all entities matching the query.
         *
         * @tparam Ts       Component types to filter by (all must satisfy IsComponent)
         * @param callback  Accumulator function: (ResultType, Ts&...) -> ResultType
         * @param seed       Initial accumulator value
         * @return Final accumulated result after processing all matching entities
         *
         * @note Each entity's components are passed to the callback, which updates the accumulator.
         *       Processing order depends on archetype iteration order.
         */
        template <typename... Ts>
            requires(IsComponent<Ts> and ...)
        auto Reduce(auto&& callback, auto seed) -> decltype(seed)
        {
            All<Ts...>();

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

        /**
         * @brief Retrieves the single entity matching all specified components.
         *
         * @tparam Ts  Component types to filter by
         *
         * @return Entity that has all specified components
         */
        template <typename... Ts>
            requires(IsComponent<Ts> and ...)
        std::optional<Entity> FindUnique()
        {
            All<Ts...>();

            std::optional<Entity> entity = std::nullopt;

            auto signature = Signature::Make<Ts...>();

            for (auto&& archetype : mComponentManager->mArchetypes)
            {
                if (mQueryFilter.MatchArchetype(archetype.get()))
                {
                    const auto count = archetype->Count();

                    if (count <= 0)
                        continue;

                    if (count > 1)
                        return std::nullopt;

                    if (entity.has_value())
                        return std::nullopt;

                    entity = archetype->First();
                }
            }

            return entity;
        }

        /**
         * @brief Returns all entities that have all specified component types.
         *
         * @tparam Ts  Component types to filter by
         * @return Vector of entity ids matching the component signature
         */
        template <typename... Ts>
            requires(IsComponent<Ts> and ...)
        std::vector<Entity> EntitiesWith()
        {
            All<Ts...>();

            auto entities = std::vector<Entity>();
            entities.reserve(Count<Ts...>());

            for (auto&& archetype : mComponentManager->mArchetypes)
            {
                if (mQueryFilter.MatchArchetype(archetype.get()))
                {
                    archetype->GetRegisteredEntities(entities);
                }
            }

            return entities;
        }

        /**
         * @brief Returns the first entity matching the query.
         *
         * @tparam Ts  Component types to filter by (all must satisfy IsComponent)
         * @return Optional containing the entity if found, std::nullopt otherwise
         */
        template <typename... Ts>
            requires(IsComponent<Ts> and ...)
        std::optional<Entity> First()
        {
            All<Ts...>();

            for (auto&& archetype : mComponentManager->mArchetypes)
            {
                if (mQueryFilter.MatchArchetype(archetype.get()))
                {
                    if (const auto entity = archetype->First(); entity.has_value())
                    {
                        return entity;
                    }
                }
            }

            return std::nullopt;
        }

        /**
         * @brief Collects all matching entities and their components into a vector of tuples.
         *
         * @tparam Ts  Component types to retrieve (all must satisfy IsComponent)
         * @return Vector of (Entity, Ts...) tuples for all matching entities
         */
        template <typename... Ts>
            requires(IsComponent<Ts> and ...)
        auto Iterate() -> std::vector<std::tuple<Entity, Ts...>>
        {
            All<Ts...>();

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

        /**
         * @brief Counts entities that have all specified component types.
         *
         * @tparam Ts  Component types to filter by
         * @return Total number of matching entities across all archetypes
         */
        template <typename... Ts>
            requires(IsComponent<Ts> and ...)
        std::size_t Count()
        {
            All<Ts...>();

            std::size_t count = 0;

            mComponentManager->ForEachArchetype([&](const Archetype* archetype) {
                if (!mQueryFilter.MatchArchetype(archetype))
                    return;

                count += archetype->Count();
            });

            return count;
        }

        /**
         * @brief Assigns a human-readable label for profiling and debugging.
         *
         * @param name  Label string to identify this query operation
         * @return Reference to this Query for chaining
         *
         * @note When FREYR_PROFILING=ON, the label appears in Perfetto traces.
         */
        Query& WithLabel(const std::string_view name)
        {
            mLabel = std::string(name);
            return *this;
        }

        /**
         * @brief Iterates over all entities with the specified component types.
         *
         * @tparam Ts  Component types to filter by
         * @param action            Callback invoked for each entity with component references
         *
         * @note Thread-safe when used with async iteration (ForAsync).
         */
        template <typename... Ts>
            requires(IsComponent<Ts> and ...)
        Query& Each(auto&& action)
        {
            All<Ts...>();
            mAction = [action = std::forward<decltype(action)>(action)](ArchetypeChunk& chunk) {
                chunk.ForEach<Ts...>(skr::type_name<decltype(action)>(), action);
            };

            Run();

            return *this;
        }

        /**
         * @brief Iterates asynchronously for chunk-level parallelization.
         *
         * @tparam Ts  Component types to filter by
         * @param action            Callback invoked per chunk with component ranges
         */
        template <typename... Ts>
            requires(IsComponent<Ts> and ...)
        Query& EachAsync(auto&& action)
        {
            All<Ts...>();
            mAction = [action = std::forward<decltype(action)>(action)](ArchetypeChunk& chunk) {
                chunk.ForEach<Ts...>(skr::type_name<decltype(action)>(), action);
            };

            Schedule();

            return *this;
        }

      protected:
        void Run() const;

        void Schedule();

        /**
         * @brief Sets the component inclusion filter for the query.
         *
         * @tparam Ts  Component types that matching entities must have (all must satisfy IsComponent)
         * @return Reference to this Query for chaining
         *
         * @note This is typically called implicitly by terminal query operations.
         *       Multiple calls replace the previous inclusion filter.
         */
        template <typename... Ts>
            requires(IsComponent<Ts> and ...)
        Query& All()
        {
            mQueryFilter.Including<Ts...>();
            return *this;
        }

        virtual void OnExecute() {}
        virtual void OnFlush() {}

      private:
        Ref<ComponentManager> mComponentManager;
        Ref<QueryAggregator>  mQueryAggregator;

        QueryFilter mQueryFilter;
        QueryAction mAction;
        std::string mLabel;
    };

} // namespace FREYR_NAMESPACE