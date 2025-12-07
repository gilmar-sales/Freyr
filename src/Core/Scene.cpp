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
        mEntityManager(serviceProvider->GetService<EntityManager>()),
        mSystemManager(serviceProvider->GetService<SystemManager>()),
        mComponentManager(serviceProvider->GetService<ComponentManager>()),
        mEventManager(serviceProvider->GetService<EventManager>()),
        mTaskManager(serviceProvider->GetService<TaskManager>())
    {
    }

    Scene::~Scene() = default;

    void Scene::BeginTrace(std::string label)
    {
        FREYR_PROFILING_BEGIN("USER", label.data(), perfetto::Track(0));
    }

    void Scene::EndTrace()
    {
        FREYR_PROFILING_END("USER", perfetto::Track(0));
    }

    void Scene::ExecuteTasks()
    {
        DestroyEntities();

        FREYR_PROFILING_BEGIN("FREYR", "StartWorkers", perfetto::Track(0));
        mTaskManager->StartWorkers();
        FREYR_PROFILING_END("FREYR", perfetto::Track(0));

        FREYR_PROFILING_BEGIN("FREYR", "StartTasks", perfetto::Track(0));
        for (auto&& archetype : mComponentManager->mArchetypes)
        {
            archetype->StartTasks();
        }
        FREYR_PROFILING_END("FREYR", perfetto::Track(0));

        FREYR_PROFILING_BEGIN("FREYR", "StopWorkers", perfetto::Track(0));
        mTaskManager->WaitForAllTasks();
        FREYR_PROFILING_END("FREYR", perfetto::Track(0));
        FREYR_PROFILING_END("FREYR", perfetto::Track(0));
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
        FREYR_PROFILING_END("FREYR", perfetto::Track(0));

        mTracingSession->StopBlocking();
        const auto trace_data = mTracingSession->ReadTraceBlocking();

        // Write the trace into a file.
        std::ofstream output;

        auto trace_name =
            std::format("freyr_trace_{}.pftrace", std::chrono::system_clock::now().time_since_epoch().count());

        output.open(trace_name.c_str(), std::ios::out | std::ios::binary);
        output.write(&trace_data[0], trace_data.size());
        output.close();
#endif // FREYR_PROFILING
    }

    void Scene::Update(float dt)
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
            cfg.add_buffers()->set_size_kb(1024 * 1024); // Record up to 1 GiB.

            auto* ds_cfg = cfg.add_data_sources()->mutable_config();
            ds_cfg->set_name("track_event");
            ds_cfg->set_track_event_config_raw(track_event_cfg.SerializeAsString());

            mTracingSession = perfetto::Tracing::NewTrace();
            mTracingSession->Setup(cfg);

            mTracingSession->StartBlocking();

            FREYR_PROFILING_BEGIN("FREYR", "Main Thread", perfetto::Track(0));

            mTaskManager->BeginProfiling();
        }

#endif // FREYR_PROFILING
        FREYR_PROFILING_BEGIN("FREYR", "Frame", perfetto::Track(0), "TotalEntities", mEntityManager->LivingEntities());

        const auto provider = mServiceProvider.lock()->CreateServiceScope()->GetServiceProvider();

        mFixedDeltaTimeAccumulator += dt;

        if (mFixedDeltaTimeAccumulator >= 2.0f)
        {
            mFixedDeltaTimeAccumulator = mOptions->FixedDeltaTime;
        }

        while (mFixedDeltaTimeAccumulator >= mOptions->FixedDeltaTime)
        {
            mFixedDeltaTimeAccumulator =
                std::min(mOptions->FixedDeltaTime, mFixedDeltaTimeAccumulator - mOptions->FixedDeltaTime) -
                mOptions->FixedDeltaTime;

            FREYR_PROFILING_BEGIN("FREYR", "PreFixedUpdate", perfetto::Track(0));
            mTaskManager->StartWorkers();
            mSystemManager->PreFixedUpdate(mOptions->FixedDeltaTime, provider);
            mTaskManager->WaitForAllTasks();
            DestroyEntities();
            FREYR_PROFILING_END("FREYR", perfetto::Track(0));

            FREYR_PROFILING_BEGIN("FREYR", "FixedUpdate", perfetto::Track(0));
            mTaskManager->StartWorkers();
            mSystemManager->FixedUpdate(mOptions->FixedDeltaTime, provider);
            mTaskManager->WaitForAllTasks();
            DestroyEntities();
            FREYR_PROFILING_END("FREYR", perfetto::Track(0));

            FREYR_PROFILING_BEGIN("FREYR", "PostFixedUpdate", perfetto::Track(0));
            mTaskManager->StartWorkers();
            mSystemManager->PostFixedUpdate(mOptions->FixedDeltaTime, provider);
            mTaskManager->WaitForAllTasks();
            DestroyEntities();
            FREYR_PROFILING_END("FREYR", perfetto::Track(0));

            mFixedDeltaTimeAccumulator -= mOptions->FixedDeltaTime;
        }

        FREYR_PROFILING_BEGIN("FREYR", "PreUpdate", perfetto::Track(0), "TotalEntities",
                              mEntityManager->LivingEntities());
        mTaskManager->StartWorkers();
        mSystemManager->PreUpdate(dt, provider);
        mTaskManager->WaitForAllTasks();
        DestroyEntities();
        FREYR_PROFILING_END("FREYR", perfetto::Track(0));

        FREYR_PROFILING_BEGIN("FREYR", "Update", perfetto::Track(0));
        mTaskManager->StartWorkers();
        mSystemManager->Update(dt, provider);
        mTaskManager->WaitForAllTasks();
        DestroyEntities();
        FREYR_PROFILING_END("FREYR", perfetto::Track(0));

        FREYR_PROFILING_BEGIN("FREYR", "PostUpdate", perfetto::Track(0));
        mTaskManager->StartWorkers();
        mSystemManager->PostUpdate(dt, provider);
        mTaskManager->WaitForAllTasks();
        DestroyEntities();
        FREYR_PROFILING_END("FREYR", perfetto::Track(0));

        FREYR_PROFILING_END("FREYR", perfetto::Track(0));
    }

    void Scene::DestroyEntities()
    {
        FREYR_PROFILING_BEGIN("FREYR", "DestroyEntities", perfetto::Track(0));
        for (auto entity : mEntitiesToDestroy)
        {
            mComponentManager->EntityDestroyed(entity);
            mEntityManager->DestroyEntity(entity);
        }
        mTaskManager->WaitForAllTasks();
        mEntitiesToDestroy.clear();
        FREYR_PROFILING_END("FREYR", perfetto::Track(0));
    }

    Ref<Archetype> Scene::AddArchetype(const Ref<Archetype>& archetype) const
    {
        return mComponentManager->AddArchetype(archetype);
    }
} // namespace FREYR_NAMESPACE