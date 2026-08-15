#pragma once

#include "Freyr/Containers/Archetype.hpp"
#include "Freyr/Core/Profiling.hpp"

#include <unordered_map>

namespace FREYR_NAMESPACE
{

    struct EntityIndex
    {
        Archetype*      archetype;
        ArchetypeChunk* archetypeChunk;
    };

    struct SignatureHash
    {
        size_t operator()(const Signature& signature) const noexcept { return signature.Hash(); }
    };

    class ComponentManager
    {
      public:
        explicit ComponentManager(const skr::Arc<FreyrOptions>&         freyrOptions,
                                  const skr::Arc<skr::ServiceProvider>& serviceProvider) :
            mMaxEntities(freyrOptions->MaxEntities), mServiceProvider(serviceProvider), mRegisteredComponents(1024)
        {
            mArchetypes.reserve(1024);
            mArchetypesBySignature.reserve(1024);
            SetMaxEntities(mMaxEntities);
        }

        ~ComponentManager()
        {
            mArchetypesBySignature.clear();
            mArchetypes.clear();
        }

        void SetMaxEntities(const Entity maxEntities) { mEntityIndexes.resize(maxEntities); }

        template <typename T>
            requires IsComponent<T>
        void RegisterComponent()
        {
            const auto componentId = GetComponentId<T>();
            if (mRegisteredComponents.contains(componentId))
                return;

            mRegisteredComponents.insert(componentId);
        }

        template <typename T>
            requires IsComponent<T>
        [[nodiscard]] bool UnregisterComponent()
        {
            return UnregisterComponent(GetComponentId<T>());
        }

        [[nodiscard]] bool UnregisterComponent(const ComponentId componentId)
        {
            if (!mRegisteredComponents.contains(componentId))
                return true;

            Signature probe;
            probe.AddComponent(componentId);

            for (const auto& archetype : mArchetypes)
            {
                if (probe.Match(archetype->GetSignature()) && archetype->Count() > 0)
                    return false;
            }

            mRegisteredComponents.remove(componentId);
            return true;
        }

        template <typename T>
            requires IsComponent<T>
        [[nodiscard]] bool IsComponentRegistered() const
        {
            return IsComponentRegistered(GetComponentId<T>());
        }

        [[nodiscard]] bool IsComponentRegistered(const ComponentId componentId) const
        {
            return mRegisteredComponents.contains(componentId);
        }

        template <typename T>
            requires IsComponent<T>
        [[nodiscard]] ComponentId GetComponentIndex() const
        {
            FREYR_ASSERT(mRegisteredComponents.contains(GetComponentId<T>()) && "Component not registered before use.");

            return mRegisteredComponents.getIndex(GetComponentId<T>());
        }

        [[nodiscard]] std::size_t ArchetypeCount() const { return mArchetypes.size(); }

        void ForEachArchetype(auto&& function) const
        {
            for (const auto& archetype : mArchetypes)
            {
                function(archetype.get());
            }
        }

        template <typename T>
            requires IsComponent<T>
        void AddComponent(const Entity entity, T component)
        {
            CreateOrUpdateEntityIndexWith<T>(entity, [entity, component](EntityIndex& entityIndex) {
                auto& [actualArchetype, actualChunk] = entityIndex;

                actualChunk->AddComponents<T>(entity, component, [](auto, auto&) {});
            });
        }

        template <typename... Ts>
            requires(IsComponent<Ts> and ...)
        void AddComponents(const Entity entity, const Ts&... components)
        {
            CreateOrUpdateEntityIndexWith<Ts...>(
                entity,
                [entity, components...](EntityIndex& entityIndex) {
                    auto& [actualArchetype, actualChunk] = entityIndex;

                    actualChunk->AddComponents<Ts...>(entity, components..., [](Entity, Ts&...) {});
                });
        }

        template <typename... Ts, typename TFunc>
            requires(IsComponent<Ts> and ...) and
                    (std::is_invocable_v<TFunc, Entity, Ts&...> or std::is_invocable_v<TFunc, Ts&...>)
        void AddComponents(const Entity entity, const Ts&... components, TFunc&& callback)
        {
            CreateOrUpdateEntityIndexWith<Ts...>(
                entity,
                [entity, components..., callback = std::forward<TFunc>(callback)](
                    EntityIndex& entityIndex) mutable {
                    auto& [actualArchetype, actualChunk] = entityIndex;

                    actualChunk->AddComponents<Ts...>(entity, components..., std::move(callback));
                });
        }

        template <typename T>
            requires IsComponent<T>
        void RemoveComponent(const Entity entity)
        {
            CreateOrUpdateEntityIndexWith<Remove<T>>(entity, [&](EntityIndex&) {});
        }

        template <typename... Ts>
            requires(IsComponent<Ts> and ...)
        void RemoveComponents(const Entity entity)
        {
            CreateOrUpdateEntityIndexWith<Remove<Ts>...>(entity, [&](EntityIndex&) {});
        }

        template <typename T>
            requires IsComponent<T>
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

            if (archetype == nullptr)
                return false;

            if (!archetype->HasComponents<Ts...>())
                return false;

            f(chunk->GetComponent<Ts>(entity)...);

            return true;
        }

        template <typename T>
            requires IsComponent<T>
        [[nodiscard]] bool HasComponent(const Entity& entity)
        {
            const auto& [archetype, _] = GetEntityIndex(entity);

            return archetype != nullptr && archetype->HasComponent<T>();
        }

        template <typename... Ts>
            requires(IsComponent<Ts> and ...)
        [[nodiscard]] bool HasComponents(const Entity& entity)
        {
            const auto& [archetype, _] = GetEntityIndex(entity);

            return archetype != nullptr && archetype->HasComponents<Ts...>();
        }

        void EntityDestroyed(const Entity& entity)
        {
            auto& entityIndex        = GetEntityIndex(entity);
            auto& [archetype, chunk] = entityIndex;

            if (chunk)
            {
                auto* chunkPtr = chunk;
                chunkPtr->EnqueueTask([chunkPtr, entity, &entityIndex] {
                    chunkPtr->RemoveEntity(entity);
                    entityIndex.archetype      = nullptr;
                    entityIndex.archetypeChunk = nullptr;
                });
                return;
            }

            archetype = nullptr;
            chunk     = nullptr;
        }

        inline EntityIndex& GetEntityIndex(const Entity& entity) { return mEntityIndexes[entity]; }

        skr::Arc<Archetype> AddArchetype(skr::Arc<Archetype> archetype)
        {
            FREYR_TRACE("FREYR", "ComponentManager::AddArchetype");

            const auto signature = archetype->GetSignature();

            if (const auto existingIt = mArchetypesBySignature.find(signature);
                existingIt != mArchetypesBySignature.end())
            {
                const auto& existingArchetype = existingIt->second;

                archetype->ForEachChunk([&](ArchetypeChunk* chunk) {
                    chunk->ForEach("ForEachEntity", [&](auto entity) {
                        GetEntityIndex(entity).archetype      = existingArchetype.get();
                        GetEntityIndex(entity).archetypeChunk = chunk;
                    });
                });

                archetype->MoveData(existingArchetype);

                return existingArchetype;
            }

            mArchetypesBySignature.emplace(signature, archetype);
            mArchetypes.push_back(archetype);

            archetype->ForEachChunk([&](ArchetypeChunk* chunk) {
                chunk->ForEach("ForEachEntity", [&](auto entity) {
                    GetEntityIndex(entity).archetype      = archetype.get();
                    GetEntityIndex(entity).archetypeChunk = chunk;
                });
            });

            return archetype;
        }

        template <typename... Components>
            requires(IsComponent<Components> and ...)
        void ForEach(const char* label, auto&& f)
        {
            const auto signature = Signature::Make<Components...>();

            for (const auto& archetype : mArchetypes)
            {
                if (signature.Match(archetype->GetSignature()))
                {
                    archetype->ForEach<Components...>(label, f);
                }
            }
        }

      private:
        template <typename... Ts>
        void CreateOrUpdateEntityIndexWith(const Entity entity, auto&& callback)
        {
            auto& entityIndex = GetEntityIndex(entity);
            auto  write       = mEntityIndexesLock.write();

            auto& [actualArchetype, actualChunk] = entityIndex;

            if (actualArchetype != nullptr)
            {
                auto signature = actualArchetype->GetSignature();
                (([&] {
                     using TComponent = std::remove_reference_t<Ts>;

                     if constexpr (is_remove<TComponent>::value)
                         signature.RemoveComponent<unwrap_remove_t<TComponent>>();
                     else
                         signature.AddComponent<TComponent>();
                 }()),
                 ...);

                if (signature.IsEmpty())
                {
                    actualChunk->EnqueueTask([actualChunk, entity, &entityIndex] {
                        actualChunk->RemoveEntity(entity);
                        entityIndex.archetype      = nullptr;
                        entityIndex.archetypeChunk = nullptr;
                    });
                    return;
                }

                if (signature != actualArchetype->GetSignature())
                {
                    skr::Arc<Archetype> newArchetype = nullptr;

                    if (const auto existingIt = mArchetypesBySignature.find(signature);
                        existingIt != mArchetypesBySignature.end())
                    {
                        newArchetype = existingIt->second;
                    }

                    if (newArchetype == nullptr)
                    {
                        newArchetype = mServiceProvider.lock()->GetService<Archetype>();
                        actualArchetype->RegisterComponentsTo<Ts...>(newArchetype);

                        (([&] {
                             using TComponent = std::remove_reference_t<Ts>;

                             if constexpr (!is_remove<TComponent>::value)
                                 newArchetype->RegisterComponent<TComponent>();
                         }()),
                         ...);
                        mArchetypesBySignature.emplace(newArchetype->GetSignature(), newArchetype);
                        mArchetypes.push_back(newArchetype);
                    }

                    auto*      oldChunk = actualChunk;
                    const auto newChunk = newArchetype->AddEntity(entity);

                    actualChunk     = newChunk;
                    actualArchetype = newArchetype.get();

                    oldChunk->EnqueueTask([oldChunk,
                                           entity,
                                           newChunk,
                                           callback = std::move(callback),
                                           &entityIndex]() mutable {
                        oldChunk->MoveData(entity, newChunk);
                        callback(entityIndex);
                    });
                    return;
                }
            }
            else
            {
                auto signature = Signature();
                (([&] {
                     if constexpr (!is_remove<Ts>::value)
                         signature.AddComponent<Ts>();
                 }()),
                 ...);

                if (signature.IsEmpty())
                    return;

                if (const auto existingIt = mArchetypesBySignature.find(signature);
                    existingIt != mArchetypesBySignature.end())
                {
                    actualArchetype = existingIt->second.get();
                }

                if (actualArchetype == nullptr)
                {
                    const auto newArchetype = mServiceProvider.lock()->GetService<Archetype>();
                    (([&] {
                         using TComponent = std::remove_reference_t<Ts>;

                         if constexpr (!is_remove<TComponent>::value)
                             newArchetype->RegisterComponent<TComponent>();
                     }()),
                     ...);
                    mArchetypesBySignature.emplace(newArchetype->GetSignature(), newArchetype);
                    mArchetypes.push_back(newArchetype);
                    actualArchetype = newArchetype.get();
                }

                actualChunk = actualArchetype->AddEntity(entity);
            }

            callback(entityIndex);
        }

        friend class Registry;
        friend class Query;

        Entity mMaxEntities;

        skr::WeakArc<skr::ServiceProvider>                    mServiceProvider;
        SparseSet<ComponentId>                               mRegisteredComponents;
        std::vector<skr::Arc<Archetype>>                     mArchetypes;
        std::unordered_map<Signature, skr::Arc<Archetype>, SignatureHash> mArchetypesBySignature;
        std::vector<EntityIndex>                             mEntityIndexes;
        RwLock                                               mEntityIndexesLock;
    };
} // namespace FREYR_NAMESPACE
