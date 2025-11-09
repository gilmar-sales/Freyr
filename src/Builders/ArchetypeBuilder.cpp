#include "Freyr/Builders/ArchetypeBuilder.hpp"

#include "Freyr/Core/Profiling.hpp"
#include "Freyr/Core/Scene.hpp"

namespace FREYR_NAMESPACE
{
    ArchetypeBuilder::ArchetypeBuilder(const Ref<skr::ServiceProvider>& serviceProvider) :
        mEntityCount(0), mEntityManager(serviceProvider->GetService<EntityManager>()),
        mTaskManager(serviceProvider->GetService<TaskManager>()), mScene(serviceProvider->GetService<Scene>()),
        mArchetype(serviceProvider->GetService<Archetype>()), mFunctions({})
    {
        mFunctions.reserve(32);
    }

    ArchetypeBuilder& ArchetypeBuilder::WithEntities(const Entity entityCount)
    {
        mEntityCount = entityCount;

        return *this;
    }

    Ref<Archetype> ArchetypeBuilder::Build()
    {
        if (mEntityCount < 1)
            return nullptr;

        FREYR_PROFILING_BEGIN("FREYR", "ArchetypeBuilder::Build", perfetto::Track(0));

        mArchetype->EnsureCapacity(mEntityCount);

        for (auto i = 0; i < mEntityCount; i++)
        {
            const auto entity = mEntityManager->CreateEntity();

            auto chunk = mArchetype->AddEntity(entity);
            for (const auto& componentRegistration : mComponentsRegistrations)
            {
                componentRegistration->f(chunk, entity);
            }
        }

        for (const auto& function : mFunctions)
        {
            function();
        }

        FREYR_PROFILING_END("FREYR", perfetto::Track(0));

        return mScene->AddArchetype(mArchetype);
    }
} // namespace FREYR_NAMESPACE
