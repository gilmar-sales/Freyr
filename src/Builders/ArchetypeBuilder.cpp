#include "Freyr/Builders/ArchetypeBuilder.hpp"

#include "Freyr/Core/ECSManager.hpp"

namespace FREYR_NAMESPACE
{
    ArchetypeBuilder::ArchetypeBuilder(ECSManager* manager) :
        mManager(manager),
        mArchetype(std::make_shared<Archetype>(manager->mMaxEntities)),
        mEntityCount(0)
    {
    }

    ArchetypeBuilder& ArchetypeBuilder::WithEntities(Entity entityCount)
    {
        mEntityCount = entityCount;

        for (auto entity = 0; entity < mEntityCount; entity++)
        {
            mArchetype->AddEntity(entity);
            mArchetype->CopyEntity(0, entity);
        }

        return *this;
    }

    std::shared_ptr<Archetype> ArchetypeBuilder::Build()
    {
        for (auto i = 0; i < mEntityCount; i++)
        {
            auto entity = mManager->CreateEntity();
            mArchetype->Swap(i, entity);
        }

        return mManager->AddArchetype(mArchetype);
    }
} // namespace FREYR_NAMESPACE
