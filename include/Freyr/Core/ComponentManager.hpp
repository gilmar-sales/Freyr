#pragma once

#include "Freyr/Containers/Archetype.hpp"
#include "Freyr/Core/Profiling.hpp"

namespace FREYR_NAMESPACE
{
    struct EntityIndex
    {
        Entity          entity {};
        Archetype*      archetype;
        ArchetypeChunk* archetypeChunk;
    };

    class ComponentManager
    {
      public:
        explicit ComponentManager(
            const Ref<FreyrOptions>&         freyrOptions,
            const Ref<skr::ServiceProvider>& serviceProvider,
            const Ref<TaskManager>&          taskManager) :
            mMaxEntities(freyrOptions->MaxEntities),
            mServiceProvider(serviceProvider), mRegisteredComponents(1024),
            mTaskManager(taskManager)
        {
            mArchetypes.reserve(1024);
            SetMaxEntities(mMaxEntities);
        }

        ~ComponentManager() { mArchetypes.clear(); }

        void SetMaxEntities(const Entity maxEntities)
        {
            mEntityIndexes.resize(maxEntities);
        }

        template <typename T>
        void RegisterComponent()
        {
            FREYR_ASSERT(!mRegisteredComponents.contains(GetComponentId<T>()) &&
                         "Registering component type more than once.");

            mRegisteredComponents.insert(GetComponentId<T>());
        }

        template <typename T>
        [[nodiscard]] ComponentId GetComponentIndex() const
        {
            FREYR_ASSERT(mRegisteredComponents.contains(GetComponentId<T>()) &&
                         "Component not registered before use.");

            return mRegisteredComponents.getIndex(GetComponentId<T>());
        }

        template <typename T>
        void AddComponent(const Entity& entity, T component)
        {
            auto& [entityA, archetype, chunk] = GetEntityIndex(entity);

            if (archetype != nullptr)
            {
                auto signature = archetype->GetSignature();
                signature.AddComponent<T>();

                if (signature != archetype->GetSignature())
                {
                    Ref<Archetype> newArchetype = nullptr;

                    for (const auto& existingArchetype : mArchetypes)
                    {
                        if (existingArchetype->GetSignature() == signature &&
                            existingArchetype)
                        {
                            newArchetype = existingArchetype;
                            break;
                        }
                    }

                    if (newArchetype == nullptr)
                    {
                        newArchetype =
                            mServiceProvider->GetService<Archetype>();
                        newArchetype->RegisterComponent<T>();
                        mArchetypes.push_back(newArchetype);
                    }

                    archetype->MoveData(entity, newArchetype);
                    newArchetype->AddComponent<T>(entity, component);
                    GetEntityIndex(entity).entity    = entity;
                    GetEntityIndex(entity).archetype = newArchetype.get();
                    GetEntityIndex(entity).archetypeChunk =
                        archetype->GetChunk(entity);
                }
            }
            else
            {
                const Signature signature = MakeSignature<T>();

                for (const auto& existingArchetype : mArchetypes)
                {
                    if (existingArchetype->GetSignature() == signature)
                    {
                        archetype = existingArchetype.get();
                        break;
                    }
                }

                if (archetype == nullptr)
                {
                    auto newArchetype =
                        mServiceProvider->GetService<Archetype>();
                    newArchetype->RegisterComponent<T>();

                    mArchetypes.push_back(newArchetype);
                    archetype = newArchetype.get();
                }

                archetype->AddComponent<T>(entity, component);
            }
        }

        template <typename... Ts>
        void AddComponents(const Entity& entity, const Ts&... component)
        {
            auto& [entityA, archetype, chunk] = GetEntityIndex(entity);

            if (archetype != nullptr)
            {
                auto signature = archetype->GetSignature();
                signature.AddComponents<Ts...>();

                if (signature != archetype->GetSignature())
                {
                    Ref<Archetype> newArchetype = nullptr;

                    for (const auto& existingArchetype : mArchetypes)
                    {
                        if (existingArchetype->GetSignature() == signature &&
                            existingArchetype)
                        {
                            newArchetype = existingArchetype;
                            break;
                        }
                    }

                    if (newArchetype == nullptr)
                    {
                        newArchetype =
                            mServiceProvider->GetService<Archetype>();
                        meta::forEach(
                            [&](auto&& c) {
                                using T = std::remove_reference_t<decltype(c)>;
                                newArchetype->RegisterComponent<T>();
                            },
                            std::make_tuple(component...));
                        mArchetypes.push_back(newArchetype);
                    }

                    archetype->MoveData(entity, newArchetype);

                    meta::forEach(
                        [&](auto&& c) {
                            using T = std::remove_reference_t<decltype(c)>;
                            newArchetype->AddComponent<T>(entity, c);
                        },
                        std::make_tuple(component...));
                    GetEntityIndex(entity).entity    = entity;
                    GetEntityIndex(entity).archetype = newArchetype.get();
                    GetEntityIndex(entity).archetypeChunk =
                        archetype->GetChunk(entity);
                }
            }
            else
            {
                const Signature signature = MakeSignature<Ts...>();

                for (const auto& existingArchetype : mArchetypes)
                {
                    if (existingArchetype->GetSignature() == signature)
                    {
                        archetype = existingArchetype.get();
                        break;
                    }
                }

                if (archetype == nullptr)
                {
                    auto newArchetype =
                        mServiceProvider->GetService<Archetype>();

                    meta::forEach(
                        [&](auto&& c) {
                            using T = std::remove_reference_t<decltype(c)>;
                            newArchetype->RegisterComponent<T>();
                        },
                        std::make_tuple(component...));

                    mArchetypes.push_back(newArchetype);
                    archetype = newArchetype.get();
                }

                meta::forEach(
                    [&](auto&& c) {
                        using T = std::remove_reference_t<decltype(c)>;
                        archetype->AddComponent<T>(entity, c);
                    },
                    std::make_tuple(component...));
            }
        }

        template <typename T>
        void RemoveComponent(const Entity& entity)
        {
            auto& [_, archetype, chunk] = GetEntityIndex(entity);

            FREYR_ASSERT(archetype != nullptr);

            archetype->RemoveComponent<T>(entity);
        }

        template <typename T>
        T& GetComponent(const Entity& entity)
        {
            auto& [entityA, archetype, chunk] = GetEntityIndex(entity);

            FREYR_ASSERT(entityA == entity && archetype != nullptr);

            return archetype->GetComponent<T>(entity);
        }

        template <typename... Ts>
        std::tuple<Ts&...> GetComponents(const Entity& entity)
        {
            auto& [entityA, archetype, chunk] = GetEntityIndex(entity);

            FREYR_ASSERT(entityA == entity && archetype != nullptr);

            return archetype->GetComponents<Ts...>(entity);
        }

        template <typename... Ts>
            requires(IsComponent<Ts> and ...)
        bool TryGetComponents(const Entity& entity, auto&& f)
        {
            auto& [entityA, archetype, chunk] = GetEntityIndex(entity);

            if (entityA != entity || archetype == nullptr || chunk == nullptr)
                return false;

            f(chunk->GetComponent<Ts>(entity)...);

            return true;
        }

        template <typename T>
        [[nodiscard]] bool HasComponent(const Entity& entity)
        {
            const auto& [entityA, archetype, chunk] = GetEntityIndex(entity);

            FREYR_ASSERT(entityA == entity && archetype != nullptr);

            return archetype->HasComponent<T>();
        }

        void EntityDestroyed(const Entity& entity)
        {
            auto& [entityA, archetype, chunk] = GetEntityIndex(entity);

            FREYR_ASSERT(entityA == entity && archetype != nullptr);

            archetype->RemoveEntity(entity);

            entityA   = -1;
            archetype = nullptr;
            chunk     = nullptr;
        }

        inline EntityIndex& GetEntityIndex(const Entity& entity)
        {
            return mEntityIndexes[entity];
        }

        Ref<Archetype> AddArchetype(Ref<Archetype> archetype)
        {
            FREYR_PROFILING_BEGIN("FREYR",
                                  "ComponentManager::AddArchetype",
                                  perfetto::Track(0));

            const auto signature = archetype->GetSignature();

            if (const auto existingArchetypeIt = std::ranges::find_if(
                    mArchetypes,
                    [&](const Ref<Archetype>& arch) {
                        return arch->GetSignature() == signature;
                    });
                existingArchetypeIt != mArchetypes.end())
            {
                archetype->MoveData(*existingArchetypeIt);

                for (const auto entity :
                     archetype->mRegisteredEntities.getDense())
                {
                    GetEntityIndex(entity).entity = entity;
                    GetEntityIndex(entity).archetype =
                        existingArchetypeIt->get();
                    GetEntityIndex(entity).archetypeChunk =
                        existingArchetypeIt->get()->GetChunk(entity);
                }

                FREYR_PROFILING_END("FREYR", perfetto::Track(0));

                return *existingArchetypeIt;
            }

            mArchetypes.push_back(archetype);

            for (const auto entity : archetype->mRegisteredEntities.getDense())
            {
                GetEntityIndex(entity).entity    = entity;
                GetEntityIndex(entity).archetype = archetype.get();
                GetEntityIndex(entity).archetypeChunk =
                    archetype->GetChunk(entity);
            }

            FREYR_PROFILING_END("FREYR", perfetto::Track(0));

            return archetype;
        }

        template <typename... Components>
            requires(IsComponent<Components> and ...)
        void ForEach(std::string label, auto&& f)
        {
            const auto signature = MakeSignature<Components...>();

            for (const auto& archetype : mArchetypes)
            {
                if (signature.Match(archetype->GetSignature()))
                {
                    archetype->ForEach<Components...>(label, f);
                }
            }
        }

      private:
        friend class Scene;

        Entity mMaxEntities;

        Ref<skr::ServiceProvider>   mServiceProvider;
        SparseSet<ComponentId>      mRegisteredComponents;
        std::vector<Ref<Archetype>> mArchetypes;
        std::vector<EntityIndex>    mEntityIndexes;
        Ref<TaskManager>            mTaskManager;
    };

} // namespace FREYR_NAMESPACE
