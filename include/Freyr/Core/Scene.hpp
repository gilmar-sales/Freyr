#pragma once

#include "Freyr/Builders/ArchetypeBuilder.hpp"
#include "Freyr/Core/ComponentManager.hpp"
#include "Freyr/Core/EntityManager.hpp"
#include "Freyr/Core/EventManager.hpp"
#include "Freyr/Core/Profiling.hpp"
#include "Freyr/Core/Query.hpp"
#include "Freyr/Core/QueryAggregator.hpp"
#include "Freyr/Core/SystemManager.hpp"
#include "Freyr/Core/TaskManager.hpp"

namespace FREYR_NAMESPACE
{
    class Scene : public std::enable_shared_from_this<Scene>
    {
      public:
        /**
         * @brief Constructs a new Scene with the given service provider.
         *
         * Initializes all managers (Entity, Component, Event, System, Task) and profiling support.
         * The Scene retains a weak reference to the service provider.
         *
         * @param serviceProvider  Skirnir service provider for dependency injection
         */
        explicit Scene(const Ref<skr::ServiceProvider>& serviceProvider);

        /**
         * @brief Destructor that cleans up all registered systems and managers.
         */
        ~Scene();

        /**
         * @brief Creates an ArchetypeBuilder for constructing complex entity archetypes.
         *
         * @return  ArchetypeBuilder instance bound to this scene's service provider
         */
        ArchetypeBuilder CreateArchetypeBuilder() const { return ArchetypeBuilder(mServiceProvider.lock()); }

        template <typename... Ts>
            requires(IsComponent<Ts> and ...)
        Entity CreateEntity()
        {
            return CreateEntity(Ts {}...);
        }

        template <typename... Ts>
            requires(IsComponent<Ts> and ...)
        Entity CreateEntity(const Ts&... components)
        {
            auto entity = mEntityManager->CreateEntity();

            if (std::tuple_size_v<std::tuple<Ts...>> == 0)
                return entity;

            mComponentManager->AddComponents<Ts...>(entity, components..., [](auto, Ts&...) {});

            return entity;
        }

        /**
         * @brief Creates an entity and invokes callback with the newly created entity.
         *
         * @tparam Ts       Component types to attach (zero or more)
         * @tparam TFunc    Callback type (deduced, must not be a component type)
         * @param callback  Function called with the entity id before components are added
         * @param components Optional component values to attach to the entity
         *
         * @note If no components are provided, callback is invoked immediately.
         *       Otherwise, callback runs after components are registered.
         */
        template <typename... Ts, typename TFunc>
            requires(IsComponent<Ts> and ...) and (not IsComponent<TFunc>)
        void CreateEntity(TFunc&& callback, const Ts&... components)
        {
            auto entity = mEntityManager->CreateEntity();

            if constexpr (std::tuple_size_v<std::tuple<Ts...>> == 0)
            {
                callback(entity);
                return;
            }

            mComponentManager->AddComponents<Ts...>(entity, components..., callback);
        }

        /**
         * @brief Schedules an entity for destruction at the end of the current frame.
         *
         * @param entity  The entity to destroy
         *
         * @note Destruction is deferred until Update() completes to avoid iterator invalidation.
         *       All queued destructions are processed in DestroyEntities() after systems run.
         */
        void DestroyEntity(const Entity& entity) { mEntitiesToDestroy.insert(entity); }

        /**
         * @brief Adds a component to an existing entity.
         *
         * @tparam T       Component type (must satisfy IsComponent)
         * @param entity   Target entity
         * @param component Value to copy/move into the entity's component storage
         */
        template <typename T>
            requires IsComponent<T>
        void AddComponent(const Entity& entity, const T& component = {})
        {
            mComponentManager->AddComponent<T>(entity, component);
        }

        /**
         * @brief Adds multiple components to an entity in a single call.
         *
         * @tparam Ts       Component types (all must satisfy IsComponent)
         * @param entity    Target entity
         * @param component Values to attach (variadic, one per type)
         */
        template <typename... Ts>
            requires(IsComponent<Ts> and ...)
        void AddComponents(const Entity entity, const Ts&... component)
        {
            mComponentManager->AddComponents<Ts...>(entity, component..., [](auto, Ts&...) {});
        }

        template <typename T>
            requires IsComponent<T>
        void RemoveComponent(const Entity entity)
        {
            mComponentManager->RemoveComponent<T>(entity);
        }
        template <typename... Ts>
            requires(IsComponent<Ts> and ...)
        void RemoveComponents(const Entity entity)
        {
            mComponentManager->RemoveComponents<Ts...>(entity);
        }

        /**
         * @brief Checks if an entity has a specific component type.
         *
         * @tparam T     Component type to query
         * @param entity  Entity to check
         * @return true if entity has the component, false otherwise
         */
        template <typename T>
            requires IsComponent<T>
        bool HasComponent(const Entity& entity) const
        {
            return mComponentManager->HasComponent<T>(entity);
        }

        template <typename... Ts>
            requires(IsComponent<Ts> and ...)
        bool HasComponents(const Entity& entity) const
        {
            return mComponentManager->HasComponents<Ts...>(entity);
        }

        /**
         * @brief Attempts to retrieve multiple components and invoke a callback if all exist.
         *
         * @tparam Ts      Component types to retrieve
         * @param entity   Entity whose components to fetch
         * @param f        Callback receiving references to all requested components
         * @return true if all components existed and callback was invoked, false otherwise
         */
        template <typename... Ts>
            requires(IsComponent<Ts> and ...)
        bool TryGetComponents(const Entity& entity, auto&& f)
        {
            return mComponentManager->TryGetComponents<Ts...>(entity, f);
        }

        template <typename T>
            requires IsComponent<T>
        ComponentId GetComponentIndex() const
        {
            return mComponentManager->GetComponentIndex<T>();
        }

        /**
         * @brief Subscribes a listener to an event type.
         *
         * @tparam T         Event type (must satisfy IsEvent)
         * @param listener   Callback function invoked when the event is sent
         * @return ListenerHandle to manage subscription lifetime
         */
        template <typename T>
            requires IsEvent<T>
        Ref<fr::ListenerHandle> AddEventListener(auto&& listener)
        {
            return mEventManager->Subscribe<T>(listener);
        }

        /**
         * @brief Dispatches an event to all registered listeners.
         *
         * @tparam T    Event type (must satisfy IsEvent)
         * @param event Value to send; listeners receive a copy
         */
        template <typename T>
            requires IsEvent<T>
        void SendEvent(T event)
        {
            mEventManager->Send(event);
        }

        /**
         * @brief Retrieves the single entity matching all specified components.
         *
         * @tparam Components  Component types to filter by
         * @return Entity that has all specified components
         *
         * @note Asserts that exactly one entity matches; use EntitiesWith() if multiple may exist.
         */
        template <typename... Components>
            requires(IsComponent<Components> and ...)
        Entity FindUnique()
        {
            auto entities = EntitiesWith<Components...>();

            FREYR_ASSERT(entities.size() == 1 && "More than 1 entity match the components");

            return entities[0];
        }

        /**
         * @brief Iterates over all entities with the specified component types.
         *
         * @tparam Components  Component types to filter by
         * @param f            Callback invoked for each entity with component references
         *
         * @note Thread-safe when used with async iteration (ForEachAsync).
         */
        template <typename... Components>
            requires(IsComponent<Components> and ...)
        void ForEach(auto&& f)
        {
            auto label = skr::type_name<std::remove_reference_t<decltype(f)>>();
            ForEach<Components...>(label, f);
        }

        /**
         * @brief Iterates over entities with profiling label for tracing.
         *
         * @tparam Components  Component types to filter by
         * @param label        Human-readable name for profiling/tracing
         * @param f            Callback invoked for each entity with component references
         */
        template <typename... Components>
            requires(IsComponent<Components> and ...)
        void ForEach(const char* label, auto&& f)
        {
            mComponentManager->ForEach<Components...>(label, f);
        }

        template <typename... Components>
        void ForEach(const char* label, SparseSet<Entity>& entities, auto&& f)
        {
            auto signature = Signature::Make<Components...>();

            for (auto&& archetype : mComponentManager->mArchetypes)
            {
                if (signature.Match(archetype->GetSignature()))
                {
                    archetype->ForEach<Components...>(label, entities, f);
                }
            }
        }

        /**
         * @brief Iterates asynchronously over entities with matching components.
         *
         * @tparam Components  Component types to filter by
         * @param f            Callback invoked for each chunk with component ranges
         *
         * @note ForEachAsync parallelizes by chunk, distributing work across threads.
         */
        template <typename... Components>
            requires(IsComponent<Components> and ...)
        void ForEachAsync(auto&& f)
        {
            auto label = skr::type_name<std::remove_reference_t<decltype(f)>>();
            ForEachAsync<Components...>(label, f);
        }

        /**
         * @brief Iterates asynchronously with profiling label for chunk-level parallelization.
         *
         * @tparam Components  Component types to filter by
         * @param label        Human-readable name for profiling/tracing
         * @param f            Callback invoked per chunk with component ranges
         */
        template <typename... Components>
            requires(IsComponent<Components> and ...)
        void ForEachAsync(const char* label, auto&& f)
        {
            auto signature = Signature::Make<Components...>();

            for (auto& archetype : mComponentManager->mArchetypes)
            {
                if (signature.Match(archetype->GetSignature()))
                {
                    archetype->ForEachAsync<Components...>(label, f);
                }
            }
        }

        /**
         * @brief Maps each entity to a value and returns a vector of results.
         *
         * @tparam Components  Component types to filter by
         * @param f             Transform function returning a value for each entity
         * @return Vector of transformed values in entity order
         */
        template <typename... Components>
            requires(IsComponent<Components> and ...)
        auto Map(auto&& f) -> std::vector<decltype(f(*(new Entity {}), *(new Components {})...))>
        {
            auto count = Count<Components...>();

            auto buffer = std::vector<decltype(f(*(new Entity {}), *(new Components {})...))>(count);

            auto signature = Signature::Make<Components...>();

            Entity index = count;

            for (auto&& archetype : mComponentManager->mArchetypes)
            {
                if (signature.Match(archetype->GetSignature()))
                {
                    index -= archetype->Count();
                    archetype->Map<Components...>(f, index, buffer);
                }
            }

            return buffer;
        }

        /**
         * @brief Counts entities that have all specified component types.
         *
         * @tparam Components  Component types to filter by
         * @return Total number of matching entities across all archetypes
         */
        template <typename... Components>
            requires(IsComponent<Components> and ...)
        std::size_t Count()
        {
            std::size_t count     = 0;
            auto        signature = Signature::Make<Components...>();

            for (auto&& archetype : mComponentManager->mArchetypes)
            {
                if (signature.Match(archetype->GetSignature()))
                {
                    count += archetype->Count();
                }
            }

            return count;
        }

        /**
         * @brief Returns all entities that have all specified component types.
         *
         * @tparam Components  Component types to filter by
         * @return Vector of entity ids matching the component signature
         */
        template <typename... Components>
            requires(IsComponent<Components> and ...)
        std::vector<Entity> EntitiesWith()
        {
            auto entities = std::vector<Entity>();
            entities.reserve(Count<Components...>());

            auto signature = Signature::Make<Components...>();

            for (auto&& archetype : mComponentManager->mArchetypes)
            {
                if (signature.Match(archetype->GetSignature()))
                {
                    archetype->GetRegisteredEntities(entities);
                }
            }

            return entities;
        }

        /**
         * @brief Advances the scene by deltaTime, processing systems and deferred destructions.
         *
         * @param deltaTime  Time elapsed since last frame in seconds
         *
         * @note Calls SystemManager::Update, then DestroyEntities() to finalize queued deletions.
         */
        void Update(float deltaTime);

        /**
         * @brief Starts Perfetto tracing session for profiling.
         *
         * @note Only available when FREYR_PROFILING=ON.
         */
        void BeginProfiling();

        /**
         * @brief Stops the active Perfetto tracing session.
         */
        void EndProfiling() const;

        /**
         * @brief Begins a named trace scope for profiling.
         *
         * @param label  Human-readable name for the trace segment
         */
        void BeginTrace(const char* label);

        /**
         * @brief Ends the current trace scope.
         */
        void EndTrace();

        /**
         * @brief Executes all pending tasks in the TaskManager.
         */
        void ExecuteTasks();

        template <typename... Ts>
            requires(IsComponent<Ts> and ...)
        Ref<Query> CreateQuery() const
        {
            const auto query = mServiceProvider.lock()->GetService<Query>();
            query->All<Ts...>();
            return query;
        }

        template <typename TQuery, typename... Ts>
            requires(std::is_base_of_v<Query, TQuery> and (IsComponent<Ts> and ...))
        Ref<TQuery> CreateQuery()
        {
            const auto query = mServiceProvider.lock()->GetService<TQuery>();
            query->template All<Ts...>();
            return query;
        }

      protected:
        Ref<Archetype> AddArchetype(const Ref<Archetype>& archetype) const;

        friend class ArchetypeBuilder;

      private:
        void DestroyEntities();

        Ref<FreyrOptions>                   mOptions;
        std::weak_ptr<skr::ServiceProvider> mServiceProvider;
        Ref<ComponentManager>               mComponentManager;
        Ref<EntityManager>                  mEntityManager;
        Ref<EventManager>                   mEventManager;
        Ref<SystemManager>                  mSystemManager;
        Ref<TaskManager>                    mTaskManager;
        Ref<QueryAggregator>                mQueryAggregator;

        SparseSet<Entity> mEntitiesToDestroy;

        bool mBeginProfiling = false;

#ifdef FREYR_PROFILING
        std::unique_ptr<perfetto::TracingSession> mTracingSession;
#endif // FREYR_PROFILING
    };

} // namespace FREYR_NAMESPACE
