#pragma once

#include "ComponentManager.hpp"
#include "Core/TaskManager.hpp"
#include "EntityManager.hpp"
#include "EventManager.hpp"
#include "Meta/Iteration.hpp"
#include "SystemManager.hpp"
#include "Types/Component.hpp"

namespace perfetto
{
    class TracingSession;
}

namespace FREYR_NAMESPACE
{

    class ECSManager
    {
      public:
        ECSManager(Entity maxEntities, SystemId maxSystems = 1024);

        ~ECSManager();

        Entity CreateEntity() { return mEntityManager->CreateEntity(); }

        void DestroyEntity(const Entity &entity)
        {
            mEntityManager->DestroyEntity(entity);

            mComponentManager->EntityDestroyed(entity);
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
        const bool HasComponent(const Entity &entity)
        {
            return mComponentManager->HasComponent<T>(entity);
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
            ForEach<Components...>(label, f);
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
            ForEachAsync<Components...>(label, f);
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
                    mTaskManager->AddTask([&, label, f = std::forward<decltype(f)>(f)]
                    {
                        archetype->ForEachAsync<Components...>(label, f);
                    });
                }
            }
        }
        
        template<typename... Components>
            requires(IsComponent<Components> and ...)
        std::size_t Count()
        {
            std::size_t count = 0;
            auto signature = GetSignature<Components...>();

            for (auto &&archetype : mComponentManager->mArchetypes)
            {
                if ((signature & archetype->GetSignature()) == signature)
                {
                    count += archetype->Count();
                }
            }

            return count;
        }

        template<typename... Components>
            requires(IsComponent<Components> and ...)
        std::vector<Entity> EntitiesWith()
        {
            auto entities = std::vector<Entity>(Count<Components...>());
            auto signature = GetSignature<Components...>();

            for (auto &&archetype : mComponentManager->mArchetypes)
            {
                if ((signature & archetype->GetSignature()) == signature)
                {
                    entities.append_range(archetype->GetRegisteredEntities());
                }
            }

            return entities;
        }

        void AddTask(auto&& f)
        {
            mTaskManager->AddTask(f);
        }

        void Update(float dt);

      private:
        std::unique_ptr<ComponentManager> mComponentManager;
        std::unique_ptr<EntityManager> mEntityManager;
        std::unique_ptr<EventManager> mEventManager;
        std::unique_ptr<SystemManager> mSystemManager;
        std::unique_ptr<TaskManager> mTaskManager;
        std::unique_ptr<perfetto::TracingSession> mTracingSession;
    };

} // namespace FREYR_NAMESPACE
