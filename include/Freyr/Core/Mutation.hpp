#pragma once

#include "Freyr/Core/ComponentManager.hpp"
#include "Freyr/Core/Filter.hpp"

#include <functional>
#include <memory>

namespace FREYR_NAMESPACE
{
    class MutationAggregator;
    using MutationAction = Action<ArchetypeChunk>;

    /**
     * @brief Pending Mutation entry for deferred execution via MutationAggregator.
     */
    struct PendingMutation
    {
        Filter                               filter;
        std::function<void(ArchetypeChunk&)> run;
        std::shared_ptr<void>                actionState;
        std::size_t                          bindingSize = 0;
        void (*bind)(ArchetypeChunk&, void*, void*)      = nullptr;
        void (*applyBound)(void*, std::size_t)            = nullptr;
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
        explicit Mutation(const skr::Arc<ComponentManager>&   componentManager,
                          const skr::Arc<MutationAggregator>& mutationAggregator);
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
            auto label = mLabel.empty() ? std::string(refl::type_name<decltype(action)>()) : mLabel;
            mAction    = [action = std::forward<decltype(action)>(action),
                       label  = std::move(label)](ArchetypeChunk& chunk) {
                chunk.ForEach<Ts...>(label.c_str(), action);
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

            using ActionType           = std::decay_t<decltype(action)>;
            constexpr bool takesEntity = std::is_invocable_v<ActionType, Entity, Ts&...>;
            auto           actionCopy  = ActionType(std::forward<decltype(action)>(action));

            struct ActionState
            {
                ActionType action;
            };

            struct Binding
            {
                ActionState*       actionState = nullptr;
                const Entity*      entities    = nullptr;
                std::tuple<Ts*...> components {};
            };

            auto actionState = std::make_shared<ActionState>(ActionState { actionCopy });

            Schedule(PendingMutation {
                .filter = mFilter,
                .run =
                    [actionCopy](ArchetypeChunk& chunk) {
                        const auto count = chunk.Count();
                        if (count == 0)
                            return;

                        const auto* entities   = chunk.GetEntitiesData();
                        auto        components = std::make_tuple(&chunk.GetComponentAt<Ts>(0)...);

                        for (std::size_t index = 0; index < count; ++index)
                        {
                            if constexpr (takesEntity)
                            {
                                actionCopy(entities[index], std::get<Ts*>(components)[index]...);
                            }
                            else
                            {
                                actionCopy(std::get<Ts*>(components)[index]...);
                            }
                        }
                    },
                .actionState = actionState,
                .bindingSize = sizeof(Binding),
                .bind =
                    [](ArchetypeChunk& chunk, void* rawActionState, void* rawBinding) {
                        new (rawBinding) Binding {
                            .actionState = static_cast<ActionState*>(rawActionState),
                            .entities    = chunk.GetEntitiesData(),
                            .components  = std::make_tuple(&chunk.GetComponentAt<Ts>(0)...),
                        };
                    },
                .applyBound =
                    [](void* rawBinding, std::size_t index) {
                        auto* binding = static_cast<Binding*>(rawBinding);
                        if constexpr (takesEntity)
                        {
                            binding->actionState->action(
                                binding->entities[index],
                                std::get<Ts*>(binding->components)[index]...);
                        }
                        else
                        {
                            binding->actionState->action(
                                std::get<Ts*>(binding->components)[index]...);
                        }
                    },
            });

            return *this;
        }

      protected:
        void Run();

        void Schedule(PendingMutation&& pendingMutation);

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
        skr::Arc<ComponentManager>   mComponentManager;
        skr::Arc<MutationAggregator> mMutationAggregator;

        std::string    mLabel;
        Filter         mFilter;
        MutationAction mAction;
    };

} // namespace FREYR_NAMESPACE
