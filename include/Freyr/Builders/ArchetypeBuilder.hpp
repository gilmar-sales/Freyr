#pragma once

#include "Freyr/Containers/Archetype.hpp"
#include "Freyr/Core/EntityManager.hpp"

namespace FREYR_NAMESPACE
{
    class Scene;

    /**
     * @brief Builder for constructing Archetypes with predefined component configurations and entity batches.
     *
     * Allows registration of components and entity creation with optional per-entity callbacks.
     * Use Scene::CreateArchetypeBuilder() to instantiate.
     */
    class ArchetypeBuilder
    {
      public:
        /**
         * @brief Constructs an ArchetypeBuilder with a service provider.
         *
         * @param serviceProvider  Skirnir service provider for dependency injection
         */
        explicit ArchetypeBuilder(const Ref<skr::ServiceProvider>& serviceProvider);

        /**
         * @brief Registers a component type and its default value for the archetype.
         *
         * @tparam T         Component type (must satisfy IsComponent)
         * @param component  Default value to assign to each entity created with this archetype
         * @return Reference to this builder for chaining
         *
         * @note If the component type is already registered, this call is ignored.
         *       The component value is copied into each entity's storage during Build().
         */
        template <typename T>
            requires IsComponent<T>
        ArchetypeBuilder& WithComponent(T component)
        {
            if (!mArchetype->HasComponent<T>())
                mArchetype->RegisterComponent<T>();

            mComponentsRegistrations.insert(new ComponentRegistration {
                .componentId = GetComponentId<T>(),
                .f           = [component = component](ArchetypeChunk* chunk, Entity entity) {
                    chunk->AddComponent<T>(entity, component);
                } });

            return *this;
        }

        /**
         * @brief Sets the number of entities to create when the archetype is built.
         *
         * @param entityCount  Number of entities to instantiate
         * @return Reference to this builder for chaining
         */
        ArchetypeBuilder& WithEntities(Entity entityCount);

        /**
         * @brief Registers a callback to be invoked on each entity with the specified components.
         *
         * @tparam Components  Component types to pass to the callback
         * @param f            Function invoked for each entity: f(Entity, Components&...)
         * @return Reference to this builder for chaining
         *
         * @note The callback is stored as a deferred function and executed during Build().
         *       Entities are created first, then callbacks run on the complete archetype.
         */
        template <typename... Components>
        ArchetypeBuilder& ForEach(auto&& f)
        {
            mFunctions.push_back([&]() {
                mArchetype->ForEach<Components...>("ArchetypeBuilder::ForEach", std::forward<decltype(f)>(f));
            });

            return *this;
        }

        /**
         * @brief Finalizes the archetype construction and creates all entities.
         *
         * @return Ref to the constructed Archetype with registered entities
         *
         * @note This creates entities immediately; ensure WithEntities() was called.
         */
        Ref<Archetype> Build();

      private:
        struct ComponentRegistration
        {
            ComponentId                                  componentId;
            std::function<void(ArchetypeChunk*, Entity)> f;

            operator size_t() { return componentId; }
        };

        SparseSet<ComponentRegistration*> mComponentsRegistrations;

        friend class Scene;
        Entity             mEntityCount;
        Ref<EntityManager> mEntityManager;
        Ref<ThreadPool>   mThreadPool;
        Ref<Scene>         mScene;
        Ref<Archetype>     mArchetype;

        std::vector<std::function<void()>> mFunctions;
    };
} // namespace FREYR_NAMESPACE