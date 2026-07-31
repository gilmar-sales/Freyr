#pragma once

#include "Freyr/Core/ComponentManager.hpp"
#include "Freyr/Core/Filter.hpp"

namespace FREYR_NAMESPACE
{
    class MutationAggregator;
    using MutationAction = Action<ArchetypeChunk>;

    /**
     * @brief Pending Mutation entry for deferred execution via MutationAggregator.
     */
    struct PendingMutation
    {
        Filter         filter;
        MutationAction action;
    };

    class Mutation
    {
      public:
        /**
         * @brief Constructs a Mutation bound to a ComponentManager and MutationAggregator.
         *
         * @param componentManager  Reference to the scene's component manager
         * @param mutationAggregator   Reference to the scene's mutation aggregator
         */
        explicit Mutation(const Ref<ComponentManager>&   componentManager,
                          const Ref<MutationAggregator>& mutationAggregator);
        virtual ~Mutation();

        /**
         * @brief Assigns a human-readable label for profiling and debugging.
         *
         * @param name  Label string to identify this mutation operation
         * @return Reference to this Mutation for chaining
         *
         * @note When FREYR_PROFILING=ON, the label appears in Perfetto traces.
         */
        Mutation& WithLabel(const std::string_view name)
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
        Mutation& Each(auto&& action)
        {
            All<Ts...>();
            mAction = [action = std::forward<decltype(action)>(action)](ArchetypeChunk& chunk) {
                chunk.ForEach<Ts...>(skr::type_name<decltype(action)>().data(), action);
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
        Mutation& EachAsync(auto&& action)
        {
            All<Ts...>();
            mAction = [action = std::forward<decltype(action)>(action)](ArchetypeChunk& chunk) {
                chunk.ForEach<Ts...>(skr::type_name<decltype(action)>().data(), action);
            };

            Schedule();

            return *this;
        }

      protected:
        void Run();

        void Schedule();

        /**
         * @brief Sets the component inclusion filter for the Mutation.
         *
         * @tparam Ts  Component types that matching entities must have (all must satisfy
         * IsComponent)
         * @return Reference to this Mutation for chaining
         *
         * @note This is typically called implicitly by terminal Mutation operations.
         *       Multiple calls replace the previous inclusion filter.
         */
        template <typename... Ts>
            requires(IsComponent<Ts> and ...)
        Mutation& All()
        {
            mFilter.Including<Ts...>();
            return *this;
        }

      private:
        Ref<ComponentManager>   mComponentManager;
        Ref<MutationAggregator> mMutationAggregator;

        std::string    mLabel;
        Filter         mFilter;
        MutationAction mAction;
    };

} // namespace FREYR_NAMESPACE