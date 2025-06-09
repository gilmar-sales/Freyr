#include "Freyr/Core/Scene.hpp"

#ifdef FREYR_PROFILING
    #include <fstream>
    #include <perfetto.h>

PERFETTO_TRACK_EVENT_STATIC_STORAGE();

#endif // FREYR_PROFILING

namespace FREYR_NAMESPACE
{
    Scene::Scene(const Ref<skr::ServiceProvider>& serviceProvider) :
        mOptions(serviceProvider->GetService<FreyrOptions>()),
        mServiceProvider(serviceProvider),
        mEntityManager(serviceProvider->GetService<EntityManager>()),
        mSystemManager(serviceProvider->GetService<SystemManager>()),
        mComponentManager(serviceProvider->GetService<ComponentManager>()),
        mEventManager(serviceProvider->GetService<EventManager>()),
        mTaskManager(serviceProvider->GetService<TaskManager>())
    {
    }

    Scene::~Scene()
    {
        mServiceProvider.reset();
    }

    void Scene::BeginTrace(std::string label)
    {
        FREYR_PROFILING_BEGIN("USER", label.data(), perfetto::Track(1));
    }

    void Scene::EndTrace()
    {
        FREYR_PROFILING_END("USER", perfetto::Track(1));
    }

    void Scene::ExecuteTasks() const
    {

        for (auto&& archetype : mComponentManager->mArchetypes)
        {
            archetype->StartTasks();
        }

        for (auto&& archetype : mComponentManager->mArchetypes)
        {
            archetype->WaitTasks();
        }
    }

    void Scene::BeginProfiling()
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

        mTaskManager->BeginProfiling();

#endif // FREYR_PROFILING
    }

    void Scene::EndProfiling() const
    {
#ifdef FREYR_PROFILING
        mTaskManager->EndProfiling();

        mTracingSession->StopBlocking();
        const auto trace_data = mTracingSession->ReadTraceBlocking();

        // Write the trace into a file.
        std::ofstream output;
        output.open("freyr.pftrace", std::ios::out | std::ios::binary);
        output.write(&trace_data[0], trace_data.size());
        output.close();
#endif // FREYR_PROFILING
    }

    void Scene::Update(float dt)
    {
        const auto provider =
            mServiceProvider->CreateServiceScope()->GetServiceProvider();

        FREYR_PROFILING_BEGIN(
            "FREYR", "Main Thread", perfetto::Track(1), "total_entities",
            mEntityManager->LivingEntities());

        FREYR_PROFILING_BEGIN("FREYR", "PreUpdate", perfetto::Track(1));
        mSystemManager->PreUpdate(dt, provider);
        ExecuteTasks();
        FREYR_PROFILING_END("FREYR", perfetto::Track(1));

        mFixedDeltaTimeAccumulator += dt;

        if (mFixedDeltaTimeAccumulator >= mOptions->FixedDeltaTime)
        {
            mFixedDeltaTimeAccumulator =
                std::min(
                    mOptions->FixedDeltaTime,
                    mFixedDeltaTimeAccumulator - mOptions->FixedDeltaTime) -
                mOptions->FixedDeltaTime;

            FREYR_PROFILING_BEGIN("FREYR", "PreFixedUpdate",
                                  perfetto::Track(1));
            mSystemManager->PreFixedUpdate(mOptions->FixedDeltaTime, provider);
            ExecuteTasks();
            FREYR_PROFILING_END("FREYR", perfetto::Track(1));

            FREYR_PROFILING_BEGIN("FREYR", "FixedUpdate", perfetto::Track(1));
            mSystemManager->FixedUpdate(mOptions->FixedDeltaTime, provider);
            ExecuteTasks();
            FREYR_PROFILING_END("FREYR", perfetto::Track(1));

            FREYR_PROFILING_BEGIN("FREYR", "PostFixedUpdate",
                                  perfetto::Track(1));
            mSystemManager->PostFixedUpdate(mOptions->FixedDeltaTime, provider);
            ExecuteTasks();
            FREYR_PROFILING_END("FREYR", perfetto::Track(1));
        }

        FREYR_PROFILING_BEGIN("FREYR", "Update", perfetto::Track(1));
        mSystemManager->Update(dt, provider);
        ExecuteTasks();
        FREYR_PROFILING_END("FREYR", perfetto::Track(1));

        FREYR_PROFILING_BEGIN("FREYR", "PostUpdate", perfetto::Track(1));
        mSystemManager->PostUpdate(dt, provider);
        ExecuteTasks();
        FREYR_PROFILING_END("FREYR", perfetto::Track(1));

        FREYR_PROFILING_END("FREYR", perfetto::Track(1));
    }

    Ref<Archetype> Scene::AddArchetype(const Ref<Archetype>& archetype) const
    {
        return mComponentManager->AddArchetype(archetype);
    }
} // namespace FREYR_NAMESPACE