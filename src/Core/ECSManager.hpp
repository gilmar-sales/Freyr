#pragma once

#include "ComponentManager.hpp"
#include "Core/TaskManager.hpp"
#include "EntityManager.hpp"
#include "EventManager.hpp"
#include "Meta/Iteration.hpp"
#include "SystemManager.hpp"
#include "Types/Component.hpp"

#include <fstream>

PERFETTO_TRACK_EVENT_STATIC_STORAGE();

namespace FREYR_NAMESPACE
{
    class ECSManager
    {
      public:
        ECSManager(Entity maxEntities, SystemId maxSystems = 1024)
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

        ~ECSManager()
        {
            mTracingSession->StopBlocking();
            std::vector<char> trace_data(mTracingSession->ReadTraceBlocking());

            // Write the trace into a file.
            std::ofstream output;
            output.open("freyr.trace", std::ios::out | std::ios::binary);
            output.write(&trace_data[0], trace_data.size());
            output.close();
        };

        Entity CreateEntity() { return mEntityManager->CreateEntity(); }

        void DestroyEntity(const Entity &entity)
        {
            mEntityManager->DestroyEntity(entity);

            mComponentManager->EntityDestroyed(entity);

            mSystemManager->EntityDestroyed(entity);
        }

        template<typename T>
            requires IsComponent<T>
        void RegisterComponent()
        {
            mComponentManager->RegisterComponent<T>();
        }

        template<typename T>
            requires IsComponent<T>
        void AddComponent(const Entity &entity, const T &component = {})
        {
            mComponentManager->AddComponent<T>(entity, component);

            auto &signature = mEntityManager->GetSignature(entity);
            signature.set(GetComponentId<T>(), true);

            mSystemManager->EntitySignatureChanged(entity, signature);
        }

        template<typename T>
            requires IsComponent<T>
        void RemoveComponent(const Entity &entity)
        {
            mComponentManager->RemoveComponent<T>(entity);

            auto &signature = mEntityManager->GetSignature(entity);
            signature.set(mComponentManager->GetComponentIndex<T>(), false);

            mSystemManager->EntitySignatureChanged(entity, signature);
        }

        template<typename T>
            requires IsComponent<T>
        T &GetComponent(const Entity &entity)
        {
            return mComponentManager->GetComponent<T>(entity);
        }

        template<typename T>
            requires IsComponent<T>
        ComponentId GetComponentIndex()
        {
            return mComponentManager->GetComponentIndex<T>();
        }

        template<typename T>
            requires IsSystem<T>
        std::shared_ptr<T> RegisterSystem()
        {
            auto system = mSystemManager->RegisterSystem<T>(this);

            return system;
        }

        template<typename T>
        //    requires IsSystem<T>
        void SetSystemSignature(Signature signature)
        {
            mSystemManager->SetSignature<T>(signature);
        }

        template<typename T>
        requires IsEvent<T>
        void AddEventListener(std::function<void(T)> const &listener)
        {
            mEventManager->AddListener<T>(listener);
        }

        template<typename T>
        requires IsEvent<T>
        void SendEvent(T event) { mEventManager->SendEvent(event); }

        template<typename... Components>
            requires(IsComponent<Components> and ...)
        void ForEach(auto &&f)
        {
            auto label = typeid(f).name();
            auto signature = GetSignature<Components...>();

            for (auto &&archetype : mComponentManager->mArchetypes)
            {
                if ((signature & archetype->GetSignature()) == signature)
                {
                    archetype->ForEach<Components...>(label, f);
                }
            }
        }

        template<typename... Components>
            requires(IsComponent<Components> and ...)
        void ForEach(std::string_view label, auto &&f)
        {
            auto signature = GetSignature<Components...>();

            for (auto &&archetype : mComponentManager->mArchetypes)
            {
                if ((signature & archetype->GetSignature()) == signature)
                {
                    archetype->ForEach<Components...>(label, f); 
                }
            }
        }

        
        template<typename... Components>
            requires(IsComponent<Components> and ...)
        void ForEachAsync(auto &&f)
        {
            auto label = typeid(f).name();
            auto signature = GetSignature<Components...>();

            for (auto &&archetype : mComponentManager->mArchetypes)
            {
                if ((signature & archetype->GetSignature()) == signature)
                {
                    mTaskManager->AddTask([&, label]
                    {
                        archetype->ForEach<Components...>(label, f); 
                    });
                }
            }
        }

        template<typename... Components>
            requires(IsComponent<Components> and ...)
        void ForEachAsync(std::string_view label, auto &&f)
        {
            auto signature = GetSignature<Components...>();

            for (auto &&archetype : mComponentManager->mArchetypes)
            {
                if ((signature & archetype->GetSignature()) == signature)
                {
                    mTaskManager->AddTask([&, label, f = std::move(f)]
                    {
                        archetype->ForEach<Components...>(label, f); 
                    });
                }
            }
        }

        void Update(float dt)
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

      private:
        std::unique_ptr<ComponentManager> mComponentManager;
        std::unique_ptr<EntityManager> mEntityManager;
        std::unique_ptr<EventManager> mEventManager;
        std::unique_ptr<SystemManager> mSystemManager;
        std::unique_ptr<TaskManager> mTaskManager;
        std::unique_ptr<perfetto::TracingSession> mTracingSession;
    };

} // namespace FREYR_NAMESPACE
