#include "Freyr/Builders/ArchetypeBuilder.hpp"

#include "Freyr/Core/Profiling.hpp"
#include "Freyr/Core/Scene.hpp"

namespace FREYR_NAMESPACE
{
    ArchetypeBuilder::ArchetypeBuilder(
        const std::shared_ptr<ServiceProvider>& serviceProvider) :
        mEntityCount(0), mScene(serviceProvider->GetService<Scene>()),
        mArchetype(serviceProvider->GetService<Archetype>()), mFunctions({})
    {
        mArchetype->AddEntity(0);
        mFunctions.reserve(32);
    }

    ArchetypeBuilder& ArchetypeBuilder::WithEntities(Entity entityCount)
    {
        mEntityCount = entityCount;

        return *this;
    }

    std::shared_ptr<Archetype> ArchetypeBuilder::Build()
    {
        if (mEntityCount < 1)
            return nullptr;

        FREYR_PROFILING_BEGIN("FREYR",
                              "ArchetypeBuilder::Build",
                              perfetto::Track((uint64_t) this));

        mArchetype->EnsureCapacity(mEntityCount);

        const auto baseEntity = mScene->CreateEntity();
        mArchetype->Swap(0, baseEntity);

        for (auto i = 1; i < mEntityCount; i++)
        {
            const auto entity = mScene->CreateEntity();
            mArchetype->AddEntity(entity);
            mArchetype->CopyEntity(baseEntity, entity);
        }

        for (const auto& function : mFunctions)
        {
            function();
        }

        FREYR_PROFILING_END("FREYR", perfetto::Track((uint64_t) this));

        return mScene->AddArchetype(mArchetype);
    }
} // namespace FREYR_NAMESPACE
