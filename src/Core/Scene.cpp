#include "Freyr/Core/Scene.hpp"

#ifdef FREYR_PROFILING
    #include <fstream>
    #include <perfetto.h>

PERFETTO_TRACK_EVENT_STATIC_STORAGE();

#endif // FREYR_PROFILING

namespace FREYR_NAMESPACE
{
    Scene::Scene(const Ref<skr::ServiceProvider>& serviceProvider) :
        mOptions(serviceProvider->GetService<FreyrOptions>()), mServiceProvider(serviceProvider),
        mComponentManager(serviceProvider->GetService<ComponentManager>()),
        mEntityManager(serviceProvider->GetService<EntityManager>()),
        mEventManager(serviceProvider->GetService<EventManager>()),
        mSystemManager(serviceProvider->GetService<SystemManager>()),
        mTaskManager(serviceProvider->GetService<TaskManager>())
    {
    }

    Scene::~Scene() = default;

    void Scene::BeginTrace(const char* label)
    {
        FREYR_TRACE_BEGIN("USER", label, perfetto::ThreadTrack::Current());
    }

    void Scene::EndTrace()
    {
        FREYR_TRACE_END("USER", perfetto::ThreadTrack::Current());
    }

    void Scene::ExecuteTasks()
    {
        {

            FREYR_TRACE("FREYR", "StartWorkers");
            mTaskManager->StartWorkers();

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

            FREYR_TRACE("FREYR", "WaitForAllTasks");
            mTaskManager->WaitForAllTasks();
        }
    }

    void Scene::BeginProfiling()
    {
#ifdef FREYR_PROFILING

        mBeginProfiling = true;

#endif // FREYR_PROFILING
    }

    void Scene::EndProfiling() const
    {
#ifdef FREYR_PROFILING

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

    void Scene::Update(float deltaTime)
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
        }

#endif // FREYR_PROFILING
        FREYR_TRACE("FREYR", "Frame");
        mEventManager->Flush();

        mTaskManager->StartWorkers();

        const auto provider = mServiceProvider.lock()->CreateServiceScope()->GetServiceProvider();

        {
            FREYR_TRACE("FREYR", "RunPipelines");
            mSystemManager->Update(deltaTime, provider);
            mTaskManager->WaitForAllTasks();
            DestroyEntities();
        }

        mTaskManager->StopWorkers();
    }

    void Scene::DestroyEntities()
    {
        FREYR_TRACE("FREYR", "DestroyEntities");
        for (auto entity : mEntitiesToDestroy)
        {
            mComponentManager->EntityDestroyed(entity);
            mEntityManager->DestroyEntity(entity);
        }

        mTaskManager->WaitForAllTasks();
        mEntitiesToDestroy.clear();
    }

    Ref<Archetype> Scene::AddArchetype(const Ref<Archetype>& archetype) const
    {
        return mComponentManager->AddArchetype(archetype);
    }
} // namespace FREYR_NAMESPACE
