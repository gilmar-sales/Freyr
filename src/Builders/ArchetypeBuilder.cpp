#include "Freyr/Builders/ArchetypeBuilder.hpp"

#include "Freyr/Core/Scene.hpp"

namespace FREYR_NAMESPACE
{
    ArchetypeBuilder::ArchetypeBuilder(const std::shared_ptr<Scene>& scene) :
        mScene(scene),
        mArchetype(std::make_shared<Archetype>(scene->mMaxEntities))
    {
    }

    std::shared_ptr<Archetype> ArchetypeBuilder::Build(Entity entityCount)
    {
        if (entityCount < 1)
            return nullptr;

        const auto first = mScene->CreateEntity();
        mArchetype->Swap(0, first);
        for (auto i = 1; i < entityCount; i++)
        {
            const auto entity = mScene->CreateEntity();
            mArchetype->AddEntity(entity);
            mArchetype->CopyEntity(first, entity);
        }

        return mScene->AddArchetype(mArchetype);
    }
} // namespace FREYR_NAMESPACE
