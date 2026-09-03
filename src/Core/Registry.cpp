#include "Freyr/Core/Registry.hpp"

#ifdef FREYR_PROFILING
    #include <perfetto.h>
#endif // FREYR_PROFILING

namespace FREYR_NAMESPACE
{
    Registry::Registry(const skr::Arc<skr::ServiceProvider>& serviceProvider) :
        mOptions(serviceProvider->GetService<FreyrOptions>()), mServiceProvider(serviceProvider),
        mComponentManager(serviceProvider->GetService<ComponentManager>()),
        mEntityManager(serviceProvider->GetService<EntityManager>()),
        mEventManager(serviceProvider->GetService<EventManager>()),
        mSystemManager(serviceProvider->GetService<SystemManager>()),
        mThreadPool(serviceProvider->GetService<ThreadPool>()),
        mMutationAggregator(serviceProvider->GetService<MutationAggregator>())
    {
    }

    Registry::~Registry() = default;

    void Registry::BeginTrace(const char* label)
    {
        FREYR_TRACE_BEGIN("USER", label);
    }

    void Registry::EndTrace()
    {
        FREYR_TRACE_END("USER");
    }

    void Registry::ExecuteTasks()
    {
        {
            mComponentManager->ExecutePendingMutations();

            FREYR_TRACE("FREYR", "StartWorkers");
            mThreadPool->StartWorkers();

            DestroyEntities();
        }

        {
            FREYR_TRACE("FREYR", "StartTasks");
            for (auto&& archetype : mComponentManager->mArchetypes)
            {
                archetype->StartTasks();
            }
        }
        mThreadPool->WaitForAllTasks();

        {

            FREYR_TRACE("FREYR", "FlushQueryAggregator");
            mMutationAggregator->Flush();
        }

        {

            FREYR_TRACE("FREYR", "WaitForAllTasks");
            mThreadPool->WaitForAllTasks();
        }
    }

    void Registry::BeginProfiling()
    {
#ifdef FREYR_PROFILING

        mBeginProfiling = true;

#endif // FREYR_PROFILING
    }

    void Registry::EndProfiling() const
    {
#ifdef FREYR_PROFILING

        FREYR_TRACE_END("FREYR");
        FreyrStopTracingSession(*mTracingSession);

#endif // FREYR_PROFILING
    }

    void Registry::Update(float deltaTime)
    {
#ifdef FREYR_PROFILING

        if (mBeginProfiling)
        {
            mBeginProfiling = false;
            mTracingSession = FreyrStartTracingSession();
            FREYR_TRACE_BEGIN("FREYR", "MainThread");
        }

#endif // FREYR_PROFILING
        FREYR_TRACE_BEGIN("FREYR", "Frame");
        mEventManager->Flush();
        mThreadPool->StartWorkers();

        mSystemManager->Accumulate(deltaTime);
        const auto provider = mServiceProvider.lock()->CreateServiceScope()->GetServiceProvider();

        mSystemManager->PreUpdate(deltaTime, provider);
        mThreadPool->WaitForAllTasks();
        mComponentManager->ExecutePendingMutations();
        DestroyEntities();

        mSystemManager->Update(deltaTime, provider);
        mThreadPool->WaitForAllTasks();
        mComponentManager->ExecutePendingMutations();
        DestroyEntities();

        mSystemManager->PostUpdate(deltaTime, provider);
        mThreadPool->WaitForAllTasks();
        mComponentManager->ExecutePendingMutations();
        DestroyEntities();

        mThreadPool->StopWorkers();
        FREYR_TRACE_END("FREYR");
    }

    void Registry::DestroyEntities()
    {
        FREYR_TRACE_BEGIN("FREYR", "DestroyEntities");
        for (auto entity : mEntitiesToDestroy)
        {
            mComponentManager->EntityDestroyed(entity);
        }

        mThreadPool->WaitForAllTasks();

        for (auto entity : mEntitiesToDestroy)
        {
            mEntityManager->DestroyEntity(entity);
        }

        mEntitiesToDestroy.clear();
        FREYR_TRACE_END("FREYR");
    }

    skr::Arc<Archetype> Registry::AddArchetype(const skr::Arc<Archetype>& archetype) const
    {
        return mComponentManager->AddArchetype(archetype);
    }

    bool Registry::UnregisterSystem(const SystemId systemId)
    {
        if (!mSystemManager->IsSystemRegistered(systemId))
            return false;

        if (const auto provider = mServiceProvider.lock())
            mSystemManager->DetachSystemService(systemId, *provider);

        return mSystemManager->UnregisterSystem(systemId);
    }

    bool Registry::UnregisterPipeline(const int32_t pipelineId)
    {
        if (!mSystemManager->HasPipeline(pipelineId))
            return false;

        const auto view    = mSystemManager->GetPipeline(pipelineId);
        const auto systems = std::vector<SystemId>(view.Systems.begin(), view.Systems.end());

        for (const auto systemId : systems)
            (void) UnregisterSystem(systemId);

        return mSystemManager->UnregisterPipeline(pipelineId);
    }
} // namespace FREYR_NAMESPACE
