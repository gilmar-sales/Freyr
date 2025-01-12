#pragma once

#include "Freyr/Containers/Archetype.hpp"
#include "Freyr/Meta/Iteration.hpp"

namespace FREYR_NAMESPACE
{
    class Scene;

    class ArchetypeBuilder
    {
      public:
        explicit ArchetypeBuilder(const std::shared_ptr<Scene>& scene);

        template <typename T>
            requires IsComponent<T>
        ArchetypeBuilder& WithDefault(T component)
        {
            if (!mArchetype->HasComponent<T>())
                mArchetype->RegisterComponent<T>();

            mArchetype->AddComponent(0, component);

            return *this;
        }

        template <typename... Components>
        ArchetypeBuilder& ForEach(auto&& f)
        {
            mArchetype->ForEach<Components...>("ArchetypeBuilder::ForEach",
                                               std::forward<decltype(f)>(f));

            return *this;
        }

        std::shared_ptr<Archetype> Build(Entity entityCount);

      private:
        friend class Scene;
        std::shared_ptr<Scene>     mScene;
        std::shared_ptr<Archetype> mArchetype;
    };
} // namespace FREYR_NAMESPACE