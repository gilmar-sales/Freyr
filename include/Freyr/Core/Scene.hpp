#pragma once

#include "Freyr/Builders/ArchetypeBuilder.hpp"
#include "Freyr/Core/ComponentManager.hpp"
#include "Freyr/Core/EntityManager.hpp"
#include "Freyr/Core/EventManager.hpp"
#include "Freyr/Core/Profiling.hpp"
#include "Freyr/Core/SystemManager.hpp"
#include "Freyr/Core/TaskManager.hpp"

namespace FREYR_NAMESPACE
{
    class Scene : public std::enable_shared_from_this<Scene>
    {
      public:
        explicit Scene(const Ref<skr::ServiceProvider>& serviceProvider);

        ~Scene();

        ArchetypeBuilder CreateArchetypeBuilder() const { return ArchetypeBuilder(mServiceProvider.lock()); }

        template <typename... Ts>
            requires(IsComponent<Ts> and ...)
        void CreateEntity(const Ts&... components)
        {
            auto entity = mEntityManager->CreateEntity();

            if (std::tuple_size_v<std::tuple<Ts...>> == 0)
                return;

            mComponentManager->AddComponents<Ts...>(entity, components..., [](auto, Ts&...) {});
        }

        template <typename... Ts, typename TFunc>
            requires(IsComponent<Ts> and ...) and (not IsComponent<TFunc>)
        void CreateEntity(TFunc&& callback, const Ts&... components)
        {
            auto entity = mEntityManager->CreateEntity();

            if constexpr (std::tuple_size_v<std::tuple<Ts...>> == 0)
            {
                callback(entity);
                return;
            }

            mComponentManager->AddComponents<Ts...>(entity, components..., callback);
        }

        void DestroyEntity(const Entity& entity) { mEntitiesToDestroy.insert(entity); }

        template <typename T>
            requires IsComponent<T>
        void AddComponent(const Entity& entity, const T& component = {})
        {
            mComponentManager->AddComponent<T>(entity, component);
        }

        template <typename... Ts>
            requires(IsComponent<Ts> and ...)
        void AddComponents(const Entity& entity, const Ts&... component)
        {
            mComponentManager->AddComponents<Ts...>(entity, component..., [](auto, Ts&...) {});
        }

        template <typename T>
            requires IsComponent<T>
        void RemoveComponent(const Entity& entity) const
        {
            mComponentManager->RemoveComponent<T>(entity);
        }

        template <typename T>
            requires IsComponent<T>
        bool HasComponent(const Entity& entity) const
        {
            return mComponentManager->HasComponent<T>(entity);
        }

        template <typename... Ts>
            requires(IsComponent<Ts> and ...)

        bool TryGetComponents(const Entity& entity, auto&& f)
        {
            return mComponentManager->TryGetComponents<Ts...>(entity, f);
        }

        template <typename T>
            requires IsComponent<T>
        ComponentId GetComponentIndex() const
        {
            return mComponentManager->GetComponentIndex<T>();
        }

        template <typename T>
            requires IsEvent<T>
        Ref<fr::ListenerHandle> AddEventListener(auto&& listener)
        {
            return mEventManager->Subscribe<T>(listener);
        }

        template <typename T>
            requires IsEvent<T>
        void SendEvent(T event)
        {
            mEventManager->Send(event);
        }

        template <typename... Components>
            requires(IsComponent<Components> and ...)
        Entity FindUnique()
        {
            auto entities = EntitiesWith<Components...>();

            FREYR_ASSERT(entities.size() == 1 && "More than 1 entity match the components");

            return entities[0];
        }

        template <typename... Components>
            requires(IsComponent<Components> and ...)
        void ForEach(auto&& f)
        {
            auto label = skr::type_name<std::remove_reference_t<decltype(f)>>();
            ForEach<Components...>(label, f);
        }

        template <typename... Components>
            requires(IsComponent<Components> and ...)
        void ForEach(std::string label, auto&& f)
        {
            mComponentManager->ForEach<Components...>(label, f);
        }

        template <typename... Components>
            requires(IsComponent<Components> and ...)
        void ForEachParallel(auto&& f)
        {
            auto label = skr::type_name<std::remove_reference_t<decltype(f)>>();
            ForEachParallel<Components...>(label, f);
        }

        template <typename... Components>
            requires(IsComponent<Components> and ...)
        void ForEachParallel(std::string label, auto&& f)
        {
            auto signature = MakeSignature<Components...>();

            Entity index = 0;
            for (auto&& archetype : mComponentManager->mArchetypes)
            {
                if (signature.Match(archetype->GetSignature()))
                {
                    index += archetype->Count();
                }
            }

            for (auto&& archetype : mComponentManager->mArchetypes)
            {
                if (signature.Match(archetype->GetSignature()))
                {
                    index -= archetype->Count();
                    archetype->ForEachParallel<Components...>(label, f, index);
                }
            }
        }

        template <typename... Components>
        void ForEach(std::string label, SparseSet<Entity>& entities, auto&& f)
        {
            auto signature = MakeSignature<Components...>();

            for (auto&& archetype : mComponentManager->mArchetypes)
            {
                if (signature.Match(archetype->GetSignature()))
                {
                    archetype->ForEach<Components...>(label, entities, f);
                }
            }
        }

        template <typename... Components>
        void ForEachParallel(std::string label, SparseSet<Entity>& entities, auto&& f)
        {
            auto signature = MakeSignature<Components...>();

            for (auto&& archetype : mComponentManager->mArchetypes)
            {
                if (signature.Match(archetype->GetSignature()))
                {
                    archetype->ForEachParallel<Components...>(label, entities, f);
                }
            }
        }

        template <typename... Components>
            requires(IsComponent<Components> and ...)
        void ForEachAsync(auto&& f)
        {
            auto label = skr::type_name<std::remove_reference_t<decltype(f)>>();
            ForEachAsync<Components...>(label, f);
        }

        template <typename... Components>
            requires(IsComponent<Components> and ...)
        void ForEachAsync(std::string label, auto&& f)
        {
            auto signature = MakeSignature<Components...>();

            for (auto& archetype : mComponentManager->mArchetypes)
            {
                if (signature.Match(archetype->GetSignature()))
                {
                    archetype->ForEachAsync<Components...>(label, f);
                }
            }
        }

        template <typename... Components>
            requires(IsComponent<Components> and ...)
        auto Map(auto&& f) -> std::vector<decltype(f(*(new Entity {}), *(new Components {})...))>
        {
            auto count = Count<Components...>();

            auto buffer = std::vector<decltype(f(*(new Entity {}), *(new Components {})...))>(count);

            auto signature = MakeSignature<Components...>();

            Entity index = count;

            for (auto&& archetype : mComponentManager->mArchetypes)
            {
                if (signature.Match(archetype->GetSignature()))
                {
                    index -= archetype->Count();
                    archetype->Map<Components...>(f, index, buffer);
                }
            }

            return buffer;
        }

        template <typename... Components>
            requires(IsComponent<Components> and ...)
        std::size_t Count()
        {
            std::size_t count     = 0;
            auto        signature = MakeSignature<Components...>();

            for (auto&& archetype : mComponentManager->mArchetypes)
            {
                if (signature.Match(archetype->GetSignature()))
                {
                    count += archetype->Count();
                }
            }

            return count;
        }

        template <typename... Components>
            requires(IsComponent<Components> and ...)
        std::vector<Entity> EntitiesWith()
        {
            auto entities = std::vector<Entity>();
            entities.reserve(Count<Components...>());

            auto signature = MakeSignature<Components...>();

            for (auto&& archetype : mComponentManager->mArchetypes)
            {
                if (signature.Match(archetype->GetSignature()))
                {
                    archetype->GetRegisteredEntities(entities);
                }
            }

            return entities;
        }

        void Update(float dt);

        void BeginProfiling();
        void EndProfiling() const;

        void BeginTrace(std::string label);
        void EndTrace();

        void ExecuteTasks();

      protected:
        Ref<Archetype> AddArchetype(const Ref<Archetype>& archetype) const;

        friend class ArchetypeBuilder;

      private:
        void DestroyEntities();

        Ref<FreyrOptions>                   mOptions;
        std::weak_ptr<skr::ServiceProvider> mServiceProvider;
        Ref<ComponentManager>               mComponentManager;
        Ref<EntityManager>                  mEntityManager;
        Ref<EventManager>                   mEventManager;
        Ref<SystemManager>                  mSystemManager;
        Ref<TaskManager>                    mTaskManager;

        SparseSet<Entity> mEntitiesToDestroy;

        bool mBeginProfiling = false;

#ifdef FREYR_PROFILING
        std::unique_ptr<perfetto::TracingSession> mTracingSession;
#endif // FREYR_PROFILING

        float mFixedDeltaTimeAccumulator = 0.0f;
    };

} // namespace FREYR_NAMESPACE
