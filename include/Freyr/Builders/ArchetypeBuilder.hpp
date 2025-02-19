#pragma once

#include "Freyr/Containers/Archetype.hpp"

#include <ServiceProvider.hpp>

namespace FREYR_NAMESPACE
{
    class Scene;

    class ArchetypeBuilder
    {
      public:
        explicit ArchetypeBuilder(
            const std::shared_ptr<ServiceProvider>& serviceProvider);

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
                mArchetype->ForEach<Components...>(
                    "ArchetypeBuilder::ForEach",
                    std::forward<decltype(f)>(f));
            });

            return *this;
        }

        std::shared_ptr<Archetype> Build();

      private:
        friend class Scene;
        Entity                             mEntityCount;
        std::shared_ptr<Scene>             mScene;
        std::shared_ptr<Archetype>         mArchetype;
        std::vector<std::function<void()>> mFunctions;
    };
} // namespace FREYR_NAMESPACE