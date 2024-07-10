#pragma once

#include "Freyr/Containers/Archetype.hpp"
#include "Freyr/Meta/Iteration.hpp"

namespace FREYR_NAMESPACE
{
    class ECSManager;

    class ArchetypeBuilder
    {
      public:
        explicit ArchetypeBuilder(ECSManager* manager);

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
            mArchetype->ForEach<Components...>("ArchetypeBuilder::ForEach",
                                               std::forward<decltype(f)>(f));

            return *this;
        }

        std::shared_ptr<Archetype> Build();

      private:
        friend class ECSManager;
        Entity                     mEntityCount;
        ECSManager*                mManager;
        std::shared_ptr<Archetype> mArchetype;
    };
} // namespace FREYR_NAMESPACE