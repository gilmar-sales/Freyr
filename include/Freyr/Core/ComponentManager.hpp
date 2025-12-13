#pragma once

#include "Freyr/Containers/Archetype.hpp"
#include "Freyr/Core/Profiling.hpp"

namespace FREYR_NAMESPACE
{
    struct EntityIndex
    {
        Archetype*      archetype;
        ArchetypeChunk* archetypeChunk;
    };

    class ComponentManager
    {
      public:
        explicit ComponentManager(const Ref<FreyrOptions>&         freyrOptions,
                                  const Ref<skr::ServiceProvider>& serviceProvider,
                                  const Ref<TaskManager>&          taskManager) :
            mMaxEntities(freyrOptions->MaxEntities), mServiceProvider(serviceProvider), mRegisteredComponents(1024)
        {
            mArchetypes.reserve(1024);
            SetMaxEntities(mMaxEntities);
        }

        ~ComponentManager() { mArchetypes.clear(); }

        void SetMaxEntities(const Entity maxEntities) { mEntityIndexes.resize(maxEntities); }

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
            FREYR_ASSERT(mRegisteredComponents.contains(GetComponentId<T>()) && "Component not registered before use.");

            return mRegisteredComponents.getIndex(GetComponentId<T>());
        }

        template <typename T>
        void AddComponent(const Entity& entity, T component)
        {
            auto& [actualArchetype, actualChunk] = GetEntityIndex(entity);

            if (actualArchetype != nullptr)
            {
                auto signature = actualArchetype->GetSignature();
                signature.AddComponent<T>();

                if (signature != actualArchetype->GetSignature())
                {
                    Ref<Archetype> newArchetype = nullptr;

                    for (const auto& existingArchetype : mArchetypes)
                    {
                        if (existingArchetype->GetSignature() == signature && existingArchetype)
                        {
                            newArchetype = existingArchetype;
                            break;
                        }
                    }

                    if (newArchetype == nullptr)
                    {
                        newArchetype = mServiceProvider.lock()->GetService<Archetype>();
                        newArchetype->RegisterComponent<T>();
                        mArchetypes.push_back(newArchetype);
                    }

                    const auto newChunk = newArchetype->AddEntity(entity);

                    actualArchetype->RegisterComponentsTo(newArchetype);

                    if (actualChunk)
                        actualChunk->MoveData(entity, newChunk);

                    newChunk->AddComponent<T>(entity, component);

                    GetEntityIndex(entity).archetype      = newArchetype.get();
                    GetEntityIndex(entity).archetypeChunk = newChunk;
                }
            }
            else
            {
                const Signature signature = MakeSignature<T>();

                for (const auto& existingArchetype : mArchetypes)
                {
                    if (existingArchetype->GetSignature() == signature)
                    {
                        actualArchetype = existingArchetype.get();
                        break;
                    }
                }

                if (actualArchetype == nullptr)
                {
                    const auto newArchetype = mServiceProvider.lock()->GetService<Archetype>();
                    newArchetype->RegisterComponent<T>();
                    mArchetypes.push_back(newArchetype);
                    actualArchetype = newArchetype.get();
                }

                actualChunk = actualArchetype->AddEntity(entity);
                actualChunk->AddComponent<T>(entity, component);
            }
        }

        template <typename... Ts>
        void AddComponents(const Entity entity, const Ts&... components, auto&& callback)
        {
            auto& [actualArchetype, actualChunk] = GetEntityIndex(entity);

            if (actualArchetype != nullptr)
            {
                auto signature = actualArchetype->GetSignature();
                signature.AddComponents<Ts...>();

                if (signature != actualArchetype->GetSignature())
                {
                    Ref<Archetype> newArchetype = nullptr;

                    for (const auto& existingArchetype : mArchetypes)
                    {
                        if (existingArchetype->GetSignature() == signature && existingArchetype)
                        {
                            newArchetype = existingArchetype;
                            break;
                        }
                    }

                    if (newArchetype == nullptr)
                    {
                        newArchetype = mServiceProvider.lock()->GetService<Archetype>();
                        meta::forEach(
                            [&]<typename TComponent>(TComponent&&) {
                                using T = std::remove_reference_t<TComponent>;
                                newArchetype->RegisterComponent<T>();
                            },
                            std::make_tuple(components...));
                        mArchetypes.push_back(newArchetype);
                    }

                    const auto newChunk = newArchetype->AddEntity(entity);

                    actualArchetype->RegisterComponentsTo(newArchetype);

                    actualChunk->MoveData(entity, newChunk);

                    actualChunk     = newChunk;
                    actualArchetype = newArchetype.get();
                    actualChunk->AddComponents<Ts...>(entity, components..., callback);
                }
            }
            else
            {
                const Signature signature = MakeSignature<Ts...>();

                for (const auto& existingArchetype : mArchetypes)
                {
                    if (existingArchetype->GetSignature() == signature)
                    {
                        actualArchetype = existingArchetype.get();
                        break;
                    }
                }

                if (actualArchetype == nullptr)
                {
                    auto newArchetype = mServiceProvider.lock()->GetService<Archetype>();

                    meta::forEach(
                        [&]<typename TComponent>(TComponent&&) {
                            using T = std::remove_reference_t<TComponent>;
                            newArchetype->RegisterComponent<T>();
                        },
                        std::make_tuple(components...));

                    mArchetypes.push_back(newArchetype);
                    actualArchetype = newArchetype.get();
                }

                actualChunk = actualArchetype->AddEntity(entity);
                actualChunk->AddComponents<Ts...>(entity, components..., callback);
            }
        }

        template <typename T>
        void RemoveComponent(const Entity& entity)
        {
            auto& [archetype, chunk] = GetEntityIndex(entity);

            FREYR_ASSERT(archetype != nullptr);

            archetype->RemoveComponent<T>(entity);
        }

        template <typename T>
        T& GetComponent(const Entity& entity)
        {
            auto& [archetype, chunk] = GetEntityIndex(entity);

            FREYR_ASSERT(archetype != nullptr && chunk != nullptr);

            return chunk->GetComponent<T>(entity);
        }

        template <typename... Ts>
            requires(IsComponent<Ts> and ...)
        bool TryGetComponents(const Entity& entity, auto&& f)
        {
            auto& [archetype, chunk] = GetEntityIndex(entity);

            if (archetype == nullptr || chunk == nullptr)
                return false;

            if (!archetype->HasComponents<Ts...>())
                return false;

            f(chunk->GetComponent<Ts>(entity)...);

            return true;
        }

        template <typename T>
        [[nodiscard]] bool HasComponent(const Entity& entity)
        {
            const auto& [archetype, _] = GetEntityIndex(entity);

            return archetype != nullptr && archetype->HasComponent<T>();
        }

        void EntityDestroyed(const Entity& entity)
        {
            auto& [archetype, chunk] = GetEntityIndex(entity);

            FREYR_ASSERT(archetype != nullptr && chunk != nullptr);

            if (chunk)
            {
                chunk->RemoveEntity(entity);
            }

            archetype = nullptr;
            chunk     = nullptr;
        }

        inline EntityIndex& GetEntityIndex(const Entity& entity) { return mEntityIndexes[entity]; }

        Ref<Archetype> AddArchetype(Ref<Archetype> archetype)
        {
            FREYR_PROFILING_BEGIN("FREYR", "ComponentManager::AddArchetype", perfetto::Track(0));

            const auto signature = archetype->GetSignature();

            if (const auto existingArchetypeIt =
                    std::ranges::find_if(mArchetypes,
                                         [&](const Ref<Archetype>& arch) { return arch->GetSignature() == signature; });
                existingArchetypeIt != mArchetypes.end())
            {

                archetype->ForEachChunk([&](ArchetypeChunk* chunk) {
                    chunk->ForEach("ForEachEntity", [&](auto entity) {
                        GetEntityIndex(entity).archetype      = existingArchetypeIt->get();
                        GetEntityIndex(entity).archetypeChunk = chunk;
                    });
                });

                archetype->MoveData(*existingArchetypeIt);

                FREYR_PROFILING_END("FREYR", perfetto::Track(0));

                return *existingArchetypeIt;
            }

            mArchetypes.push_back(archetype);

            archetype->ForEachChunk([&](ArchetypeChunk* chunk) {
                chunk->ForEach("ForEachEntity", [&](auto entity) {
                    GetEntityIndex(entity).archetype      = archetype.get();
                    GetEntityIndex(entity).archetypeChunk = chunk;
                });
            });

            FREYR_PROFILING_END("FREYR", perfetto::Track(0));

            return archetype;
        }

        template <typename... Components>
            requires(IsComponent<Components> and ...)
        void ForEach(const char* label, auto&& f)
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

        std::weak_ptr<skr::ServiceProvider> mServiceProvider;
        SparseSet<ComponentId>              mRegisteredComponents;
        std::vector<Ref<Archetype>>         mArchetypes;
        std::vector<EntityIndex>            mEntityIndexes;
    };
} // namespace FREYR_NAMESPACE