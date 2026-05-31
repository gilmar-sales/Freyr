#include "Freyr/Core/Registry.hpp"

#ifdef FREYR_PROFILING
    #include <fstream>
    #include <perfetto.h>

PERFETTO_TRACK_EVENT_STATIC_STORAGE();

#endif // FREYR_PROFILING

namespace FREYR_NAMESPACE
{
    Registry::Registry(const Ref<skr::ServiceProvider>& serviceProvider) :
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
        FREYR_TRACE_BEGIN("USER", label, perfetto::ThreadTrack::Current());
    }

    void Registry::EndTrace()
    {
        FREYR_TRACE_END("USER", perfetto::ThreadTrack::Current());
    }

    void Registry::ExecuteTasks()
    {
        {

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

        FREYR_TRACE_END("FREYR", perfetto::ProcessTrack::Current());

        mTracingSession->StopBlocking();
        const auto trace_data = mTracingSession->ReadTraceBlocking();

        std::ofstream output;

        auto trace_name =
            std::format("freyr_trace_{}.pftrace", std::chrono::system_clock::now().time_since_epoch().count());

        output.open(trace_name.c_str(), std::ios::out | std::ios::binary);
        output.write(&trace_data[0], trace_data.size());
        output.close();
#endif // FREYR_PROFILING
    }

    void Registry::Update(float deltaTime)
    {
#ifdef FREYR_PROFILING

        if (mBeginProfiling)
        {
            mBeginProfiling = false;
            auto args       = perfetto::TracingInitArgs();
            args.backends |= perfetto::kInProcessBackend;

            perfetto::Tracing::Initialize(args);
            perfetto::TrackEvent::Register();

            perfetto::protos::gen::TrackEventConfig track_event_cfg;

            perfetto::TraceConfig cfg;
            cfg.add_buffers()->set_size_kb(1024 * 1024);

            auto* ds_cfg = cfg.add_data_sources()->mutable_config();
            ds_cfg->set_name("track_event");
            ds_cfg->set_track_event_config_raw(track_event_cfg.SerializeAsString());

            mTracingSession = perfetto::Tracing::NewTrace();
            mTracingSession->Setup(cfg);

            mTracingSession->StartBlocking();
            FREYR_TRACE_BEGIN("FREYR", "MainThread", perfetto::ProcessTrack::Current());
        }

#endif // FREYR_PROFILING
        FREYR_TRACE_BEGIN("FREYR", "Frame", perfetto::Track(0, perfetto::ProcessTrack::Current()));
        mEventManager->Flush();
        mThreadPool->StartWorkers();

        mSystemManager->Accumulate(deltaTime);
        const auto provider = mServiceProvider.lock()->CreateServiceScope()->GetServiceProvider();

        mSystemManager->PreUpdate(deltaTime, provider);
        mThreadPool->WaitForAllTasks();
        DestroyEntities();

        mSystemManager->Update(deltaTime, provider);
        mThreadPool->WaitForAllTasks();
        DestroyEntities();

        mSystemManager->PostUpdate(deltaTime, provider);
        mThreadPool->WaitForAllTasks();
        DestroyEntities();

        mThreadPool->StopWorkers();
        FREYR_TRACE_END("FREYR", perfetto::Track(0));
    }

    void Registry::DestroyEntities()
    {
        FREYR_TRACE_BEGIN("FREYR", "DestroyEntities", perfetto::Track(0, perfetto::ProcessTrack::Current()));
        for (auto entity : mEntitiesToDestroy)
        {
            mComponentManager->EntityDestroyed(entity);
            mEntityManager->DestroyEntity(entity);
        }

        mThreadPool->WaitForAllTasks();
        mEntitiesToDestroy.clear();
        FREYR_TRACE_END("FREYR", perfetto::Track(0));
    }

    Ref<Archetype> Registry::AddArchetype(const Ref<Archetype>& archetype) const
    {
        return mComponentManager->AddArchetype(archetype);
    }
} // namespace FREYR_NAMESPACE
