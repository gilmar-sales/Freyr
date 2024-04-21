#include "Freyr/Core/ECSManager.hpp"

#ifdef FREYR_PROFILING
    #include <fstream>
    #include <perfetto.h>

PERFETTO_TRACK_EVENT_STATIC_STORAGE();

#endif // FREYR_PROFILING

namespace FREYR_NAMESPACE
{
    ECSManager::ECSManager(Entity maxEntities, SystemId maxSystems)
    {
        mDIContainer      = std::make_shared<DIContainer>();
        mSystemManager    = std::make_unique<SystemManager>(maxSystems, mDIContainer);
        mComponentManager = std::make_unique<ComponentManager>(maxEntities);
        mEntityManager    = std::make_unique<EntityManager>(maxEntities);
        mEventManager     = std::make_unique<EventManager>();
        mTaskManager      = std::make_unique<TaskManager>();

        StartProfiling();
    }

    ECSManager::~ECSManager()
    {
        EndProfiling();
    }

    void ECSManager::StartTraceProfiling(std::string_view label)
    {
        FREYR_PROFILING_BEGIN("USER", label.data(), perfetto::Track(2));
    }

    void ECSManager::EndTraceProfiling()
    {
        FREYR_PROFILING_END("USER", perfetto::Track(2));
    }

    void ECSManager::StartProfiling()
    {
#ifdef FREYR_PROFILING
        auto args = perfetto::TracingInitArgs();
        args.backends |= perfetto::kInProcessBackend;

        perfetto::Tracing::Initialize(args);
        perfetto::TrackEvent::Register();

        perfetto::protos::gen::TrackEventConfig track_event_cfg;

        perfetto::TraceConfig cfg;
        cfg.add_buffers()->set_size_kb(2048); // Record up to 1 MiB.

        auto* ds_cfg = cfg.add_data_sources()->mutable_config();
        ds_cfg->set_name("track_event");
        ds_cfg->set_track_event_config_raw(track_event_cfg.SerializeAsString());

        mTracingSession = perfetto::Tracing::NewTrace();
        mTracingSession->Setup(cfg);
        mTracingSession->StartBlocking();

#endif // FREYR_PROFILING
    }

    void ECSManager::EndProfiling()
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

    void ECSManager::Update(float dt)
    {
#ifdef FREYR_PROFILING

#endif // FREYR_PROFILING

        FREYR_PROFILING_BEGIN("FREYR", "Main Thread", perfetto::Track(1));
        FREYR_PROFILING_BEGIN("FREYR", "PreUpdate", perfetto::Track(1));
        mSystemManager->PreUpdate(dt);
        mTaskManager->WaitTasks();
        FREYR_PROFILING_END("FREYR", perfetto::Track(1));

        FREYR_PROFILING_BEGIN("FREYR", "Update", perfetto::Track(1));
        mSystemManager->Update(dt);
        mComponentManager->StartTracing();
        mTaskManager->WaitTasks();
        mComponentManager->EndTracing();
        FREYR_PROFILING_END("FREYR", perfetto::Track(1));

        FREYR_PROFILING_BEGIN("FREYR", "PostUpdate", perfetto::Track(1));
        mSystemManager->PostUpdate(dt);
        mTaskManager->WaitTasks();
        FREYR_PROFILING_END("FREYR", perfetto::Track(1));

        FREYR_PROFILING_END("FREYR", perfetto::Track(1));
    }
} // namespace FREYR_NAMESPACE