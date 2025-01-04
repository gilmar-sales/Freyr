#include "Freyr/Core/Scene.hpp"

#include <iostream>

#ifdef FREYR_PROFILING
    #include <fstream>
    #include <perfetto.h>

PERFETTO_TRACK_EVENT_STATIC_STORAGE();

#endif // FREYR_PROFILING

namespace FREYR_NAMESPACE
{
    Scene::Scene(Entity maxEntities, SystemId maxSystems) :
        mMaxEntities(maxEntities)
    {
        mServiceCollection = std::make_shared<ServiceCollection>();
        mSystemManager     = std::make_unique<SystemManager>(maxSystems);
        mComponentManager  = std::make_unique<ComponentManager>(maxEntities);
        mEntityManager     = std::make_unique<EntityManager>(maxEntities);
        mEventManager      = std::make_unique<EventManager>();
        mTaskManager       = std::make_unique<TaskManager>();

        StartProfiling();
    }

    Scene::~Scene()
    {
        EndProfiling();
    }

    void Scene::StartTraceProfiling(std::string_view label)
    {
        FREYR_PROFILING_BEGIN("USER", label.data(), perfetto::Track(2));
    }

    void Scene::EndTraceProfiling()
    {
        FREYR_PROFILING_END("USER", perfetto::Track(2));
    }

    void Scene::StartProfiling()
    {
#ifdef FREYR_PROFILING
        auto args = perfetto::TracingInitArgs();
        args.backends |= perfetto::kInProcessBackend;

        perfetto::Tracing::Initialize(args);
        perfetto::TrackEvent::Register();

        perfetto::protos::gen::TrackEventConfig track_event_cfg;

        perfetto::TraceConfig cfg;
        cfg.add_buffers()->set_size_kb(1024 * 1024); // Record up to 1 GiB.

        auto* ds_cfg = cfg.add_data_sources()->mutable_config();
        ds_cfg->set_name("track_event");
        ds_cfg->set_track_event_config_raw(track_event_cfg.SerializeAsString());

        mTracingSession = perfetto::Tracing::NewTrace();
        mTracingSession->Setup(cfg);

        mTracingSession->StartBlocking();

#endif // FREYR_PROFILING
    }

    void Scene::EndProfiling()
    {
#ifdef FREYR_PROFILING
        mTracingSession->StopBlocking();
        std::vector<char> trace_data(mTracingSession->ReadTraceBlocking());

        // Write the trace into a file.
        std::ofstream output;
        output.open("freyr.pftrace", std::ios::out | std::ios::binary);
        output.write(&trace_data[0], trace_data.size());
        output.close();
#endif // FREYR_PROFILING
    }

    void Scene::Update(float dt)
    {
#ifdef FREYR_PROFILING

        #endif // FREYR_PROFILING
        if (mServiceProvider == nullptr)
        {
            mServiceCollection->AddSingleton(shared_from_this());
            mServiceProvider = mServiceCollection->CreateServiceProvider();
        }

        auto provider = mServiceProvider
                            ->CreateServiceScope()
                            ->GetServiceProvider();

        FREYR_PROFILING_BEGIN("FREYR", "Main Thread", perfetto::Track(1));
        FREYR_PROFILING_BEGIN("FREYR", "PreUpdate", perfetto::Track(1));
        mSystemManager->PreUpdate(dt, provider);
        mTaskManager->WaitTasks();
        FREYR_PROFILING_END("FREYR", perfetto::Track(1));

        FREYR_PROFILING_BEGIN("FREYR", "Update", perfetto::Track(1));
        mSystemManager->Update(dt, provider);
        mComponentManager->StartTracing();
        mTaskManager->WaitTasks();
        mComponentManager->EndTracing();
        FREYR_PROFILING_END("FREYR", perfetto::Track(1));

        FREYR_PROFILING_BEGIN("FREYR", "PostUpdate", perfetto::Track(1));
        mSystemManager->PostUpdate(dt, provider);
        mTaskManager->WaitTasks();
        FREYR_PROFILING_END("FREYR", perfetto::Track(1));

        FREYR_PROFILING_END("FREYR", perfetto::Track(1));
    }

    std::shared_ptr<Archetype> Scene::AddArchetype(
        std::shared_ptr<Archetype> archetype)
    {
        return mComponentManager->AddArchetype(archetype);
    }
} // namespace FREYR_NAMESPACE