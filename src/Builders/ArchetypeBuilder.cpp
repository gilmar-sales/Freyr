#include "Freyr/Builders/ArchetypeBuilder.hpp"

#include "Freyr/Core/Profiling.hpp"
#include "Freyr/Core/Registry.hpp"

namespace FREYR_NAMESPACE
{
    ArchetypeBuilder::ArchetypeBuilder(const Ref<skr::ServiceProvider>& serviceProvider) :
        mEntityCount(0), mEntityManager(serviceProvider->GetService<EntityManager>()),
        mThreadPool(serviceProvider->GetService<ThreadPool>()), mRegistry(serviceProvider->GetService<Registry>()),
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

        FREYR_TRACE("FREYR", "ArchetypeBuilder::Build");

        mArchetype->EnsureCapacity(mEntityCount);

        for (auto i = 0; i < mEntityCount; i++)
        {
            const auto entity = mEntityManager->CreateEntity();

            const auto chunk = mArchetype->AddEntity(entity);

            for (const auto& componentRegistration : mComponentsRegistrations)
            {
                componentRegistration->f(chunk, entity);
            }
        }

        for (const auto& function : mFunctions)
        {
            function();
        }

        auto archetype = mRegistry->AddArchetype(mArchetype);

        return archetype;
    }
} // namespace FREYR_NAMESPACE
