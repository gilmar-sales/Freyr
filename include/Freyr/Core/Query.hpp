#pragma once

#include "Freyr/Core/ComponentManager.hpp"
#include "Freyr/Core/Filter.hpp"
#include "Freyr/Core/FilteredArchetypeView.hpp"
#include "Freyr/Meta/CallableComponents.hpp"
#include "Freyr/Meta/EntityOptionalInvoke.hpp"

#include <tuple>
#include <type_traits>
#include <vector>

namespace FREYR_NAMESPACE
{

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
         */
        explicit Query(const skr::Arc<ComponentManager>& componentManager);
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
            mFilter.Excluding<Ts...>();
            return *this;
        }

        /**
         * @brief Maps each matching entity through a callback, deducing components from it.
         *
         * Optional leading Entity must be typed. Remaining parameters must be concrete components.
         */
        template <typename F>
        auto Transform(F&& callback)
        {
            return TransformFromTuple(std::forward<F>(callback),
                                      typename meta::components_tuple_t<std::decay_t<F>> {});
        }

        /**
         * @brief Maps each entity to a value and returns a vector of results.
         *
         * Component types are deduced from the callable. Optional leading Entity must be typed.
         */
        template <typename F>
        auto Map(F&& f)
        {
            return MapFromTuple(std::forward<F>(f),
                                typename meta::components_tuple_t<std::decay_t<F>> {});
        }

        /**
         * @brief Accumulates values across all matching entities.
         *
         * Component types are deduced from parameters after the accumulator argument.
         * Signature: (Acc, Components&...) -> Acc
         */
        template <typename F, typename Seed>
        auto Reduce(F&& callback, Seed seed) -> Seed
        {
            return ReduceFromTuple(
                std::forward<F>(callback), std::move(seed),
                typename meta::components_tuple_after_first_t<std::decay_t<F>> {});
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

            ForEachMatchingArchetype(*mComponentManager, mFilter, [&](Archetype* archetype) {
                const auto count = archetype->Count();

                if (count <= 0)
                    return;

                if (count > 1)
                {
                    entity = std::nullopt;
                    return;
                }

                if (entity.has_value())
                {
                    entity = std::nullopt;
                    return;
                }

                entity = archetype->First();
            });

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

            ForEachMatchingArchetype(*mComponentManager, mFilter, [&](Archetype* archetype) {
                archetype->GetRegisteredEntities(entities);
            });

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

            std::optional<Entity> result = std::nullopt;

            ForEachMatchingArchetype(*mComponentManager, mFilter, [&](Archetype* archetype) {
                if (result.has_value())
                    return;

                result = archetype->First();
            });

            return result;
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

            ForEachMatchingArchetype(*mComponentManager, mFilter, [&](Archetype* archetype) {
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

            ForEachMatchingArchetype(*mComponentManager, mFilter, [&](const Archetype* archetype) {
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

      protected:
        template <typename F, typename... Ts>
            requires(IsComponent<Ts> and ...)
        auto TransformFromTuple(F&& callback, std::tuple<Ts...>)
        {
            All<Ts...>();

            static_assert(meta::callback_invocable_v<F, Ts...>,
                          "Callback must accept either (Entity, Ts...) or (Ts...)");

            using ResultType = decltype(meta::invoke_with_optional_entity_result(
                std::declval<F>(), std::declval<Entity>(), std::declval<Ts&>()...));
            auto results     = std::vector<ResultType>();

            ForEachMatchingArchetype(*mComponentManager, mFilter, [&](Archetype* archetype) {
                archetype->ForEach<Ts...>(
                    mLabel.data(), [&](Entity entity, Ts&... components) mutable {
                        results.push_back(meta::invoke_with_optional_entity_result(
                            callback, entity, components...));
                    });
            });

            return results;
        }

        template <typename F, typename... Ts>
            requires(IsComponent<Ts> and ...)
        auto MapFromTuple(F&& f, std::tuple<Ts...>)
        {
            All<Ts...>();

            static_assert(meta::callback_invocable_v<F, Ts...>,
                          "Callback must accept either (Entity, Ts...) or (Ts...)");

            const auto count = CountFromFilter();

            if constexpr (meta::callback_takes_entity_v<F, Ts...>)
            {
                using ResultType = decltype(f(std::declval<Entity>(), std::declval<Ts&>()...));
                auto results     = std::vector<ResultType>(count);

                Entity index = static_cast<Entity>(count);

                ForEachMatchingArchetype(*mComponentManager, mFilter, [&](Archetype* archetype) {
                    index -= static_cast<Entity>(archetype->Count());
                    archetype->Map<Ts...>(f, index, results);
                });

                return results;
            }
            else
            {
                using ResultType = decltype(f(std::declval<Ts&>()...));
                auto results     = std::vector<ResultType>();
                results.reserve(count);

                ForEachMatchingArchetype(*mComponentManager, mFilter, [&](Archetype* archetype) {
                    archetype->ForEach<Ts...>(
                        mLabel.data(),
                        [&](Entity entity, Ts&... components) mutable {
                            results.push_back(
                                meta::invoke_with_optional_entity_result(f, entity, components...));
                        });
                });

                return results;
            }
        }

        template <typename F, typename Seed, typename... Ts>
            requires(IsComponent<Ts> and ...)
        auto ReduceFromTuple(F&& callback, Seed seed, std::tuple<Ts...>) -> Seed
        {
            All<Ts...>();

            Seed accumulator = std::move(seed);

            ForEachMatchingArchetype(*mComponentManager, mFilter, [&](Archetype* archetype) {
                archetype->ForEach<Ts...>(mLabel.data(), [&](Ts&... components) {
                    accumulator = callback(accumulator, components...);
                });
            });

            return accumulator;
        }

        /**
         * @brief Sets the component inclusion filter for the query.
         *
         * @tparam Ts  Component types that matching entities must have (all must satisfy
         * IsComponent)
         * @return Reference to this Query for chaining
         *
         * @note This is typically called implicitly by terminal query operations.
         *       Multiple calls replace the previous inclusion filter.
         */
        template <typename... Ts>
            requires(IsComponent<Ts> and ...)
        Query& All()
        {
            mFilter.Including<Ts...>();
            return *this;
        }

        std::size_t CountFromFilter()
        {
            std::size_t count = 0;

            ForEachMatchingArchetype(*mComponentManager, mFilter, [&](const Archetype* archetype) {
                count += archetype->Count();
            });

            return count;
        }

      private:
        skr::Arc<ComponentManager> mComponentManager;
        Filter                     mFilter;
        std::string                mLabel;
    };

} // namespace FREYR_NAMESPACE
