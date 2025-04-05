#pragma once

#include "Freyr/Containers/Archetype.hpp"

namespace FREYR_NAMESPACE
{
    class Scene;

    class ArchetypeBuilder
    {
      public:
        explicit ArchetypeBuilder(
            const Ref<skr::ServiceProvider>& serviceProvider);

        template <typename T>
            requires IsComponent<T>
        ArchetypeBuilder& WithDefault(T component)
        {
            if (!mArchetype->HasComponent<T>())
                mArchetype->RegisterComponent<T>();

            mArchetype->AddComponent(0, component);

            return *this;
        }

        ArchetypeBuilder& WithEntities(Entity entityCount);

        template <typename... Components>
        ArchetypeBuilder& ForEach(auto&& f)
        {
            mFunctions.push_back([&]() {
                auto latch = mArchetype->CreateLatch();

                mArchetype->ForEach<Components...>(
                    "ArchetypeBuilder::ForEach", std::forward<decltype(f)>(f),
                    latch);

                mTaskManager->NotifyWorkers();

                if (!latch->try_wait())
                    latch->wait();
            });

            return *this;
        }

        Ref<Archetype> Build();

      private:
        friend class Scene;
        Entity                             mEntityCount;
        Ref<TaskManager>                   mTaskManager;
        Ref<Scene>                         mScene;
        Ref<Archetype>                     mArchetype;
        std::vector<std::function<void()>> mFunctions;
    };
} // namespace FREYR_NAMESPACE