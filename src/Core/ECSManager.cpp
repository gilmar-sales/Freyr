#include "ECSManager.hpp"

#include <perfetto.h>
#include <fstream>

PERFETTO_TRACK_EVENT_STATIC_STORAGE();

namespace FREYR_NAMESPACE
{
    ECSManager::ECSManager(Entity maxEntities, SystemId maxSystems)
    {
        mComponentManager = std::make_unique<ComponentManager>(maxEntities);
        mEntityManager    = std::make_unique<EntityManager>(maxEntities);
        mSystemManager    = std::make_unique<SystemManager>(maxSystems);
        mEventManager     = std::make_unique<EventManager>();
        mTaskManager      = std::make_unique<TaskManager>();

        
        auto args = perfetto::TracingInitArgs();
        args.backends |= perfetto::kInProcessBackend;

        perfetto::Tracing::Initialize(args);
        perfetto::TrackEvent::Register();

        perfetto::protos::gen::TrackEventConfig track_event_cfg;

        perfetto::TraceConfig cfg;
        cfg.add_buffers()->set_size_kb(1024);  // Record up to 1 MiB.

        auto* ds_cfg = cfg.add_data_sources()->mutable_config();
        ds_cfg->set_name("track_event");
        ds_cfg->set_track_event_config_raw(track_event_cfg.SerializeAsString());

        mTracingSession = perfetto::Tracing::NewTrace();
        mTracingSession->Setup(cfg);
        mTracingSession->StartBlocking();
    }

    ECSManager::~ECSManager()
    {
        mTracingSession->StopBlocking();
        std::vector<char> trace_data(mTracingSession->ReadTraceBlocking());

        // Write the trace into a file.
        std::ofstream output;
        output.open("freyr.trace", std::ios::out | std::ios::binary);
        output.write(&trace_data[0], trace_data.size());
        output.close();
    }
    void ECSManager::Update(float dt)
    {
        TRACE_EVENT_BEGIN("ECS", "Main Thread", perfetto::Track(1));
        TRACE_EVENT_BEGIN("ECS", "PreUpdate", perfetto::Track(1));
        mSystemManager->PreUpdate();
        mTaskManager->StartTasks();
        mTaskManager->WaitTasks();
        TRACE_EVENT_END("ECS", perfetto::Track(1));
    
        TRACE_EVENT_BEGIN("ECS", "Update", perfetto::Track(1));
        mSystemManager->Update(dt);
        mComponentManager->StartTracing();
        mTaskManager->StartTasks();
        mTaskManager->WaitTasks();
        mComponentManager->EndTracing();
        TRACE_EVENT_END("ECS", perfetto::Track(1));
    
        TRACE_EVENT_BEGIN("ECS", "PostUpdate", perfetto::Track(1));
        mSystemManager->PostUpdate();
        mTaskManager->StartTasks();
        mTaskManager->WaitTasks();
        TRACE_EVENT_END("ECS", perfetto::Track(1));

        TRACE_EVENT_END("ECS", perfetto::Track(1));
    }
} // namespace FREYR_NAMESPACE