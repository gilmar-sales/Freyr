#pragma once

#include "Freyr/Containers/Archetype.hpp"
#include "Freyr/Core/EntityManager.hpp"

namespace FREYR_NAMESPACE
{
    class Scene;

    class ArchetypeBuilder
    {
      public:
        explicit ArchetypeBuilder(const Ref<skr::ServiceProvider>& serviceProvider);

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

        ArchetypeBuilder& WithEntities(Entity entityCount);

        template <typename... Components>
        ArchetypeBuilder& ForEach(auto&& f)
        {
            mFunctions.push_back([&]() {
                mArchetype->ForEach<Components...>("ArchetypeBuilder::ForEach", std::forward<decltype(f)>(f));
            });

            return *this;
        }

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
        Ref<TaskManager>   mTaskManager;
        Ref<Scene>         mScene;
        Ref<Archetype>     mArchetype;

        std::vector<std::function<void()>> mFunctions;
    };
} // namespace FREYR_NAMESPACE