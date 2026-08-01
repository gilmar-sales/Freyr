#pragma once

#include "Freyr/Builders/ArchetypeBuilder.hpp"
#include "Freyr/Core/ComponentManager.hpp"
#include "Freyr/Core/EntityManager.hpp"
#include "Freyr/Core/EventManager.hpp"
#include "Freyr/Core/MutationAggregator.hpp"
#include "Freyr/Core/Profiling.hpp"
#include "Freyr/Core/Query.hpp"
#include "Freyr/Core/SystemManager.hpp"
#include "Freyr/Core/ThreadPool.hpp"

namespace FREYR_NAMESPACE
{
    class Registry : public std::enable_shared_from_this<Registry>
    {
      public:
        /**
         * @brief Constructs a new Registry with the given service provider.
         *
         * Initializes all managers (Entity, Component, Event, System, Task) and profiling support.
         * The Registry retains a weak reference to the service provider.
         *
         * @param serviceProvider  Skirnir service provider for dependency injection
         */
        explicit Registry(const skr::Arc<skr::ServiceProvider>& serviceProvider);

        /**
         * @brief Destructor that cleans up all registered systems and managers.
         */
        ~Registry();

        /**
         * @brief Creates an ArchetypeBuilder for constructing complex entity archetypes.
         *
         * @return  ArchetypeBuilder instance bound to this registry's service provider
         */
        ArchetypeBuilder CreateArchetypeBuilder() const
        {
            return ArchetypeBuilder(mServiceProvider.lock());
        }

        /**
         * @brief Creates a new entity with zero components.
         *
         * @return Entity handle to the newly created entity
         *
         * @note The returned entity is valid but has no components attached.
         *       Use AddComponent or AddComponents to attach data.
         */
        template <typename... Ts>
            requires(IsComponent<Ts> and ...)
        Entity CreateEntity()
        {
            return CreateEntity(Ts {}...);
        }

        /**
         * @brief Creates a new entity with the specified components.
         *
         * @tparam Ts       Component types to attach to the entity
         * @param components Values to copy/move into the entity's component storage
         * @return Entity handle to the newly created entity
         *
         * @note Components are added via ComponentManager::AddComponents with an empty callback.
         *       The entity is immediately valid and queryable after this call.
         */
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

        /**
         * @brief Removes a component from an entity.
         *
         * @tparam T       Component type to remove (must satisfy IsComponent)
         * @param entity   Target entity
         *
         * @note Removing a component may cause the entity to move to a different archetype.
         */
        template <typename T>
            requires IsComponent<T>
        void RemoveComponent(const Entity entity)
        {
            mComponentManager->RemoveComponent<T>(entity);
        }

        /**
         * @brief Removes multiple components from an entity in a single call.
         *
         * @tparam Ts       Component types to remove (all must satisfy IsComponent)
         * @param entity    Target entity
         *
         * @note All specified components are removed atomically relative to entity movement.
         */
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

        /**
         * @brief Checks if an entity has all specified component types.
         *
         * @tparam Ts     Component types to query (all must satisfy IsComponent)
         * @param entity  Entity to check
         * @return true if entity has all components, false otherwise
         */
        template <typename... Ts>
            requires(IsComponent<Ts> and ...)
        bool HasComponents(const Entity& entity) const
        {
            return mComponentManager->HasComponents<Ts...>(entity);
        }

        /**
         * @brief Retrieves multiple components and invokes callback if all exist.
         *
         * @tparam Ts       Component types to retrieve (all must satisfy IsComponent)
         * @param entity    Entity whose components to fetch
         * @param callback   Function receiving references to all requested components
         * @return true if all components existed and callback was invoked, false otherwise
         *
         * @note The callback is only called if the entity possesses all specified components.
         */
        template <typename... Ts>
            requires(IsComponent<Ts> and ...)
        bool TryGetComponents(const Entity& entity, auto&& callback)
        {
            return mComponentManager->TryGetComponents<Ts...>(entity, callback);
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
        skr::Arc<fr::ListenerHandle> AddEventListener(auto&& listener)
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
         * @brief Advances the registry by deltaTime, processing systems and deferred destructions.
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
         * @brief Executes all pending tasks in the ThreadPool.
         */
        void ExecuteTasks();

        /**
         * @brief Creates a new Query instance for entity searching.
         *
         * @return Arc to a Query bound to this registry's ComponentManager
         *
         * @note The Query is retrieved from the service provider and tied to the registry's
         *       component registry for archetype-based filtering.
         */
        skr::Arc<Query> CreateQuery() const
        {
            const auto query = mServiceProvider.lock()->GetService<Query>();
            return query;
        }

        skr::Arc<Mutation> CreateMutation() const
        {
            const auto mutation = mServiceProvider.lock()->GetService<Mutation>();
            return mutation;
        }

        /**
         * @brief Creates a specialized Query subtype.
         *
         * @tparam TQuery   Query subclass type (must derive from Query)
         * @return Arc to the specialized query instance
         */
        template <typename TQuery>
            requires(std::is_base_of_v<Query, TQuery>)
        skr::Arc<TQuery> CreateQuery()
        {
            return mServiceProvider.lock()->GetService<TQuery>();
        }

      protected:
        skr::Arc<Archetype> AddArchetype(const skr::Arc<Archetype>& archetype) const;

        friend class ArchetypeBuilder;

      private:
        void DestroyEntities();

        skr::Arc<FreyrOptions>             mOptions;
        skr::WeakArc<skr::ServiceProvider> mServiceProvider;
        skr::Arc<ComponentManager>         mComponentManager;
        skr::Arc<EntityManager>            mEntityManager;
        skr::Arc<EventManager>             mEventManager;
        skr::Arc<SystemManager>            mSystemManager;
        skr::Arc<ThreadPool>               mThreadPool;
        skr::Arc<MutationAggregator>       mMutationAggregator;

        SparseSet<Entity> mEntitiesToDestroy;

        bool mBeginProfiling = false;

#ifdef FREYR_PROFILING
        std::unique_ptr<perfetto::TracingSession> mTracingSession;
#endif // FREYR_PROFILING
    };

} // namespace FREYR_NAMESPACE
